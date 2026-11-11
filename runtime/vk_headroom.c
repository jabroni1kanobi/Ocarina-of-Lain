// VRAM headroom: the next VMA block of a memory type is allocated on a background thread, not on the render thread.
//
// VMA places an allocation in an existing block of its memory type and allocates a new block (vkAllocateMemory, 32 to
// 256 MiB) only when none has room. On the render thread that took 8-19 ms per block, several times in the first
// minute of play while VRAM grows (textures from create_tex, mirrors). A new VkDeviceMemory made by any other thread
// (VMA's pfnAllocate callback) wakes this thread for its memory type, which it then checks every 500 ms. A check
// allocates half a default block of that type (the largest size VMA still places in blocks, not in dedicated memory).
// When a block has that much room the allocation lands there and is freed at once. When none has, VMA allocates a new
// block here, and the allocation stays as that block's anchor: VMA deletes an empty block at the next free of its
// type (VmaBlockVector::Free, "we have another empty block"), so a spare block left empty lasted until the next
// texture went. The previous anchor is freed then, which gives its half block to the render thread, so at most half
// a block per memory type is held back.
// Under VRAM pressure no headroom is kept: the spare block counts against the budget that decides whether a pool gets
// its VRAM mirror (vk_memory.c), so while use plus two blocks passes 85% of the budget the anchor goes and nothing new
// is allocated.
// NX2NAVI_VRAM_HEADROOM=0 turns it off.
// NX2NAVI_VRAM_HEADROOM_BLOCKS=<n> (1-8, default 1 = the above): keep n half blocks of free room per memory type, with up
// to n anchors. A scene cut that makes thousands of textures at once (the intro's Deku Tree cut: 3,544 new textures in
// one frame, lab pfi4) outran one half block: the render thread allocated new blocks and VMA freed and remade empty ones
// in that frame (vkAllocateMemory and VmaDeviceMemoryBlock::Destroy 67 of its samples). The pressure rule scales: room
// for n + 1 blocks under 85 % of the budget, else the anchors go.

#include <stdlib.h>
#include <string.h>

#include "cfg.h"
#include "flight.h"
#include "host.h"
#include "prof.h"
#include "vk.h"
#include "platform/platform.h"

#define HR_PERIOD_MS 500
#define HR_SMALL_HEAP (1ull << 30)   // VMA_SMALL_HEAP_MAX_SIZE: smaller heaps use blocks of a heap eighth

static struct {
    int on, stop;
    PlatMutex lock;             // held around each top-up; vk_headroom_stop takes it before the allocator goes
    int started;
    PlatEvent ev;               // auto-reset: another thread allocated, or stop
    volatile uint32_t tid;      // the headroom thread's id
    volatile int64_t types;    // memory types a thread other than this one allocated device memory of
    volatile int64_t made;     // blocks this thread brought in
    volatile int64_t other;    // device memory of 32 MiB or more other threads allocated (VMA blocks, large dedicated)
    VmaAllocation anchor[64][8]; // per memory type: half blocks that keep the newest spare blocks from being deleted,
    int nanchor[64];             // oldest first
    int want;                    // NX2NAVI_VRAM_HEADROOM_BLOCKS
} H;

// Render-target pools (NX2NAVI_RT_POOLS=1, a test switch; vk_texture.c create_tex asks vk_rt_pool_alloc). The game makes
// its transient render targets anew all the time (a texture entry is keyed by guest address + descriptor, and the
// engine's allocator moves them), so images of 0.1 to 17 MiB are created and destroyed every frame. In VMA's default
// pool that needs a new 256 MiB block now and then, and VMA creates a block inside the allocation that found no room
// and under its pool lock: on the render thread, inside a recording, 5.5-6.6 ms each (lab fx4: "vkAllocateMemory 256.0
// MiB of type 0 by the RENDER thread" at the owner's middle stutter, tick ~5690; the headroom above is off there, the
// heap being over its 85 % rule with the 4 GiB texture pool). A block made by this thread in the same pool would hold
// the render thread's allocations just as long (the pool lock: vk_memory.c, staging pool). So render targets get
// pools of ONE block each: this thread makes the next pool while the newest is still unused, an allocation tries the
// pools in turn and never makes a block, and a pool is never given back (the released targets kept by vk_texture.c
// fill them to a plateau). With no pool that has room the image takes the default pool as before.
#define RTP_MAX 40
#define RTP_BLOCK (256ull << 20)
#define RTP_IMAGE_MAX (64ull << 20)    // larger images keep the default pool
#define RTP_MARGIN (512ull << 20)      // of the heap's budget left free when a pool is added
static struct {
    int on;
    VmaPool pool[RTP_MAX];
    volatile int32_t n;        // pools made
    volatile int32_t type;     // their memory type; -1 until the first render target asked
    volatile int32_t hint;     // the pool the last allocation came from
    volatile int32_t kick;     // the newest pool is in use, or an allocation found no room
    volatile int64_t taken, missed;
    int full_logged;
} RP = {.type = -1};

static void rp_kick(void) {
    if (!H.started) return;
    __atomic_store_n(&RP.kick, 1, __ATOMIC_RELEASE);
    plat_event_set(&H.ev);
}

int vk_rt_pools_on(void) { return RP.on && H.started; }

int vk_rt_pool_alloc(const VkMemoryRequirements *mr, VmaAllocation *out) {
    if (!RP.on || !H.started) return 0;
    VkCtx *c = vk_ctx();
    int32_t type = __atomic_load_n(&RP.type, __ATOMIC_ACQUIRE);
    if (type < 0) {   // the first render target names the memory type; the pools come from the headroom thread
        VmaAllocationCreateInfo ai = {.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
        uint32_t t;
        if (vmaFindMemoryTypeIndex(c->vma, mr->memoryTypeBits, &ai, &t) != VK_SUCCESS) return 0;
        int32_t none = -1;
        __atomic_compare_exchange_n(&RP.type, &none, (int32_t)t, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
        rp_kick();
        return 0;
    }
    if (!(mr->memoryTypeBits >> type & 1) || mr->size > RTP_IMAGE_MAX) return 0;
    int n = __atomic_load_n(&RP.n, __ATOMIC_ACQUIRE), h = __atomic_load_n(&RP.hint, __ATOMIC_RELAXED);
    for (int k = -1; k < n; k++) {   // the pool that had room last, then the oldest first (the newest stays the spare)
        int i = k < 0 ? h : k;
        if (i >= n || (k >= 0 && i == h)) continue;
        VmaAllocationCreateInfo ai = {.pool = RP.pool[i]};
        if (vmaAllocateMemory(c->vma, mr, &ai, out, NULL) != VK_SUCCESS) continue;
        __atomic_store_n(&RP.hint, i, __ATOMIC_RELAXED);
        if (i == n - 1) rp_kick();   // the spare is in use now: the next one, off this thread
        plat_inc64(&RP.taken);
        FX_COUNT(FX_RTPOOL, 1);
        return 1;
    }
    plat_inc64(&RP.missed);
    rp_kick();
    return 0;
}

// The headroom thread, H.lock held: one more pool when the newest is in use and the heap has the room.
static void rp_top_up(const VkCtx *c) {
    int32_t type = __atomic_load_n(&RP.type, __ATOMIC_ACQUIRE);
    if (type < 0) return;
    __atomic_store_n(&RP.kick, 0, __ATOMIC_RELEASE);
    int n = RP.n;
    if (n >= RTP_MAX) return;
    if (n) {
        VmaStatistics st;
        vmaGetPoolStatistics(c->vma, RP.pool[n - 1], &st);
        if (!st.allocationCount) return;   // the spare is still unused
    }
    VmaBudget b[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(c->vma, b);
    const VmaBudget *hb = &b[c->mem_props.memoryTypes[type].heapIndex];
    if (!hb->budget || hb->usage + RTP_BLOCK + RTP_MARGIN > hb->budget) {
        if (!RP.full_logged++)
            rt_log("vk: render-target pools: no room for pool %d (%llu of %llu MiB in use); new targets take the default pool",
                   n + 1, (unsigned long long)(hb->usage >> 20), (unsigned long long)(hb->budget >> 20));
        return;
    }
    VmaPoolCreateInfo pi = {.memoryTypeIndex = (uint32_t)type, .blockSize = RTP_BLOCK, .minBlockCount = 1, .maxBlockCount = 1};
    VmaPool p;
    if (vmaCreatePool(c->vma, &pi, &p) != VK_SUCCESS) return;
    RP.pool[n] = p;
    __atomic_store_n(&RP.n, n + 1, __ATOMIC_RELEASE);
    RP.full_logged = 0;
    rt_log("vk: render-target pools: pool %d made ahead (256 MiB, memory type %d; %lld images placed, %lld found no room so far)",
           n + 1, (int)type, (long long)RP.taken, (long long)RP.missed);
}

static void rp_destroy(const VkCtx *c) {   // before the allocator goes; the textures went first (vk_texture.c)
    for (int i = 0; i < RP.n; i++) vmaDestroyPool(c->vma, RP.pool[i]);
    RP.n = 0;
}

static void VKAPI_PTR on_allocate(VmaAllocator a, uint32_t type, VkDeviceMemory m, VkDeviceSize size, void *user) {
    (void)a, (void)m, (void)size, (void)user;
    FX_COUNT(FX_DEV_ALLOCS, 1);
    if (flight_x_on && flight_x_frame > 3000) {   // NX2NAVI_FLIGHT_X: every device memory allocation in play, and who made it
        uint32_t tid = plat_thread_id();
        rt_log("fx: frame %llu: vkAllocateMemory %.1f MiB of type %u by the %s thread", (unsigned long long)flight_x_frame + 1,
               size / 1048576.0, type, tid == H.tid ? "headroom" : tid == prof_render_tid ? "RENDER" : tid == prof_main_tid ? "MAIN" : "another");
    }
    if (!H.on || type >= 64) return;
    if (plat_thread_id() == H.tid) {
        plat_inc64(&H.made);
        return;
    }
    if (size >= 32ull << 20) plat_inc64(&H.other);
    plat_or64(&H.types, (int64_t)(1ull << type));
    plat_event_set(&H.ev);
}

const VmaDeviceMemoryCallbacks *vk_headroom_callbacks(void) {
    static const VmaDeviceMemoryCallbacks cb = {.pfnAllocate = on_allocate};
    const char *v = cfg_get("NX2NAVI_VRAM_HEADROOM");   // empty is unset: the key has no settings row for cfg_bool
    H.on = !v || !*v || cfg_bool("NX2NAVI_VRAM_HEADROOM");
    const char *n = cfg_get("NX2NAVI_VRAM_HEADROOM_BLOCKS");
    H.want = n && *n ? atoi(n) : 1;
    if (H.want < 1) H.want = 1;
    if (H.want > 8) H.want = 8;
    const char *rp = cfg_get("NX2NAVI_RT_POOLS");
    RP.on = H.on && rp && *rp && strcmp(rp, "0");
    return H.on ? &cb : NULL;
}

static VkDeviceSize block_size(const VkCtx *c, uint32_t type) {
    VkDeviceSize heap = c->mem_props.memoryHeaps[c->mem_props.memoryTypes[type].heapIndex].size;
    return heap <= HR_SMALL_HEAP ? heap / 8 : 256ull << 20;   // VMA's default preferredLargeHeapBlockSize
}

static int room(const VkCtx *c, uint32_t type, VkDeviceSize block, int blocks) {
    VmaBudget b[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(c->vma, b);
    const VmaBudget *h = &b[c->mem_props.memoryTypes[type].heapIndex];
    return h->budget && h->usage + (VkDeviceSize)(blocks + 1) * block <= h->budget / 100 * 85;
}

static void drop_oldest(const VkCtx *c, uint32_t type) {
    if (!H.nanchor[type]) return;
    vmaFreeMemory(c->vma, H.anchor[type][0]);
    memmove(&H.anchor[type][0], &H.anchor[type][1], sizeof H.anchor[type][0] * (size_t)--H.nanchor[type]);
}

static void drop_anchor(const VkCtx *c, uint32_t type) {
    while (H.nanchor[type]) drop_oldest(c, type);
}

static void top_up(const VkCtx *c, uint32_t type) {
    VkDeviceSize block = block_size(c, type);
    if (!room(c, type, block, 1)) {
        drop_anchor(c, type);
        return;
    }
    VkMemoryRequirements req = {.size = block / 2, .alignment = 256, .memoryTypeBits = 1u << type};
    VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_UNKNOWN};
    // up to `want` half blocks of free room: probes that land in room already there are held until the loop ends (so
    // each next probe looks for more room) and then freed; probes that made a block stay as its anchor
    VmaAllocation probe[8];
    int np = 0, made = 0;
    for (int k = 0; k < H.want && (k == 0 || room(c, type, block, k + 1)); k++) {
        VmaAllocation a;
        int64_t before = H.made;
        if (vmaAllocateMemory(c->vma, &req, &ai, &a, NULL) != VK_SUCCESS) break;
        if (H.made == before) {
            probe[np++] = a;
            continue;
        }
        made++;
        if (H.nanchor[type] == H.want) drop_oldest(c, type);
        H.anchor[type][H.nanchor[type]++] = a;
    }
    for (int i = 0; i < np; i++) vmaFreeMemory(c->vma, probe[i]);
    static volatile int32_t logged;
    if (made && plat_inc32(&logged) <= 32)
        rt_log("vk: headroom: %d %.0f MiB block(s) of memory type %u allocated ahead (%lld so far; %lld allocations of "
               "32 MiB or more on other threads; %d anchors kept)", made, (double)block / 1048576.0, type,
               (long long)H.made, (long long)H.other, H.nanchor[type]);
}

static void headroom_main(void *arg) {
    (void)arg;
    __atomic_store_n(&H.tid, plat_thread_id(), __ATOMIC_RELEASE);
    plat_thread_priority(PLAT_PRIO_BELOW_NORMAL);
    uint64_t watch = 0;   // memory types seen so far
    for (;;) {
        if (watch) plat_event_wait_ns(&H.ev, (uint64_t)HR_PERIOD_MS * 1000000u);
        else plat_event_wait(&H.ev);
        watch |= (uint64_t)plat_xchg64(&H.types, 0);
        plat_mutex_lock(&H.lock);
        const VkCtx *c = H.stop ? NULL : vk_ctx();
        for (uint32_t t = 0; c && t < c->mem_props.memoryTypeCount && t < 64; t++)
            if (watch >> t & 1) top_up(c, t);
        if (c && RP.on) rp_top_up(c);
        int stop = H.stop;
        if (stop && (c = vk_ctx()))
            for (uint32_t t = 0; t < 64; t++) drop_anchor(c, t);
        plat_mutex_unlock(&H.lock);
        if (stop) return;
    }
}

// Before the allocator is destroyed: waits for a running top-up, frees the anchors, and the thread allocates no more.
void vk_headroom_stop(void) {
    if (!H.started) return;
    plat_mutex_lock(&H.lock);
    H.stop = 1;
    const VkCtx *c = vk_ctx();
    for (uint32_t t = 0; c && t < 64; t++) drop_anchor(c, t);
    if (c) rp_destroy(c);
    plat_mutex_unlock(&H.lock);
    plat_event_set(&H.ev);
}

void vk_headroom_start(void) {
    if (!H.on || H.started) return;
    plat_mutex_init(&H.lock);
    plat_event_init(&H.ev, 0, 0);
    if (!plat_thread_spawn(headroom_main, NULL, "vk headroom", 0)) {
        H.on = 0;
        rt_log("vk: headroom: no thread, new VMA blocks stay on the allocating thread");
        return;
    }
    H.started = 1;
    rt_log("vk: headroom: the next VMA block of each memory type in use is allocated on a background thread "
           "(NX2NAVI_VRAM_HEADROOM=0 turns it off)");
    if (RP.on)
        rt_log("vk: render-target pools on (NX2NAVI_RT_POOLS): render targets take memory from one-block pools made "
               "ahead on the headroom thread, never from a block made inside a recording");
}
