// NX2NAVI_DRAWSET=<dir>: a ledger of what each frame drew, for finding objects that vanish for a frame
// (nx2navi-tools/drawset.py diffs it). For every frame in NX2NAVI_DRAWSET_FRAMES=<lo>-<hi> (default all), one file
// <dir>/f<frame>.bin holds a record per draw, mesh draw, indirect draw, dispatch, draw the replay skipped (with its
// reason) and samples-passed report (with the count written for it), in replay order. Indirect records carry the
// arguments as the GPU saw them: mesh and dispatch arguments are copied into a host-visible ring next to the copies the
// replay makes anyway (outside a pass), indexed indirect ones are read from guest memory once the frame's last
// submission completed (exact with NX2NAVI_MEMORY=import, where the GPU writes guest memory). A report's count is the
// one the completion thread wrote into guest memory (vk_exec_xfer.c oq_cb).
//
// The replay thread fills the records; the completion thread finishes a frame after its last fence and a writer thread
// writes it, so neither the replay nor the completion thread waits on the disk.
//
// NX2NAVI_DRAWSET_DROP=<frame>:<text>: the first draw of that frame whose key (drawset.py's, printed in the log) contains
// text (or whose kind:prog:vb:ib:caller key does) is not recorded into the command buffer: a planted vanish the differ must report.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfg.h"
#include "gfx.h"
#include "host.h"
#include "nvn_api.h"
#include "vk.h"
#include "vk_exec.h"
#include "platform/platform.h"

typedef struct {
    uint32_t ord;        // position in the frame
    uint16_t pass;       // passes begun in the frame before it (a draw: its pass is this minus 1)
    uint8_t kind;        // VK_DS_*
    uint8_t prim;        // NVN primitive of a draw
    uint32_t count, inst;   // vertices or indices, instances; mesh: workgroups; dispatch: x, y (first: z)
    int32_t first;       // first vertex, base vertex (indexed), first workgroup, dispatch z
    uint32_t binst;      // base instance
    uint32_t slot;       // argument ring slot or report cell, UINT32_MAX none
    uint32_t why;        // a skipped draw: its reason + 1
    uint64_t prog;       // program serial
    uint64_t color, depth;   // guest addresses of colour target 0 and the depth target
    uint64_t vb, ib, ind;    // vertex buffer 0, index buffer, indirect arguments or report address
    uint64_t caller;     // game call site (mesh draws, dispatches)
    uint64_t res;        // hash of the vertex (mesh: task and mesh) stage's uniform buffer addresses
    uint64_t place;      // that stage's c7 address (the object's place block)
    uint64_t tex;        // fragment texture handle 0
    uint32_t a[4];       // GPU-seen indirect arguments (a[0..2]); report: count low, high; a draw's a[3]: its
                         // fragment shader invocations (UINT32_MAX - 1: query unavailable, UINT32_MAX: not counted)
} DsRec;
_Static_assert(sizeof(DsRec) == 128, "drawset.py reads 128-byte records");

typedef struct DsFrame {
    struct DsFrame *next;
    uint64_t frame, gfx_frame, tick;
    int64_t world_us;
    uint32_t n, cap;
    DsRec *r;
    uint32_t *q;         // per record: its fragment query, UINT32_MAX none
} DsFrame;

#define RING (1u << 18)   // argument slots of 16 bytes, and report cells
int vk_drawset_on;
static int state;         // 0 not read, 1 on, -1 off
static char dir[512];
static uint64_t lo, hi = UINT64_MAX, cur = 1;   // cur: the frame being replayed (presents so far + 1)
static uint64_t drop_frame;
static char drop_text[128];
static int dropped;
static DsFrame *fr;
static uint64_t last_fence, pass_base;
static VkBuffer rb;
static volatile uint32_t *rb_map;
static uint32_t rb_next, cell_next;
static volatile uint64_t cells[RING];
static PlatMutex q_lock = PLAT_MUTEX_INIT;
static PlatCond q_cv = PLAT_COND_INIT;
static DsFrame *q_head, *q_tail;
static uint64_t n_frames, n_recs, n_bytes, write_ns;
// Fragment counts: a pipeline statistics query (fragment shader invocations) around every draw of a recorded frame,
// so drawset.py can tell a vanish of an object that was on screen from one of an object hidden anyway. The game's
// occlusion counting uses occlusion queries, a different type, so both can be active at once.
#define QN (1u << 15)
static VkQueryPool qpool;
static volatile uint8_t q_busy[QN];
static uint32_t q_next;
static int q_open;        // the newest record's query is active

static void writer(void *arg) {
    (void)arg;
    for (;;) {
        plat_mutex_lock(&q_lock);
        while (!q_head) plat_cond_wait(&q_cv, &q_lock);
        DsFrame *f = q_head;
        q_head = f->next;
        if (!q_head) q_tail = NULL;
        plat_mutex_unlock(&q_lock);
        uint64_t t0 = kern_now_ns();
        char path[600];
        snprintf(path, sizeof path, "%s/f%06llu.bin", dir, (unsigned long long)f->frame);
        FILE *o = fopen(path, "wb");
        if (o) {
            // version 2: + script tick and world clock (frame_step_world_us; -1 before the save's first frame)
            uint32_t h[16] = {0x54455344u /* DSET */, 2, (uint32_t)f->frame, (uint32_t)(f->frame >> 32), f->n,
                              (uint32_t)sizeof(DsRec), (uint32_t)f->gfx_frame, (uint32_t)(f->gfx_frame >> 32),
                              (uint32_t)f->tick, (uint32_t)(f->tick >> 32), (uint32_t)(uint64_t)f->world_us,
                              (uint32_t)((uint64_t)f->world_us >> 32)};
            fwrite(h, sizeof h, 1, o);
            if (f->n) fwrite(f->r, sizeof(DsRec), f->n, o);
            fclose(o);
        }
        n_frames++, n_recs += f->n, n_bytes += 64 + (uint64_t)f->n * sizeof(DsRec);
        write_ns += kern_now_ns() - t0;
        if (n_frames % 600 == 0)
            rt_log("drawset: %llu frames written, %llu records, %.1f MB, %.2f ms per frame on the writer",
                   (unsigned long long)n_frames, (unsigned long long)n_recs, n_bytes / 1e6, write_ns / 1e6 / (double)n_frames);
        free(f->r);
        free(f->q);
        free(f);
    }
}

// Completion thread, after the frame's last submission: fill in what the GPU produced, hand the frame to the writer.
static void frame_done(void *arg) {
    DsFrame *f = arg;
    for (uint32_t i = 0; i < f->n; i++) {
        DsRec *r = &f->r[i];
        if (r->kind == VK_DS_REPORT) {
            if (r->slot != UINT32_MAX) {
                uint64_t v = cells[r->slot % RING];
                r->a[0] = (uint32_t)v, r->a[1] = (uint32_t)(v >> 32);
            }
        } else if (r->slot != UINT32_MAX && rb_map) {
            memcpy(r->a, (const void *)(rb_map + 4 * (r->slot % RING)), 16);
        } else if (r->kind == VK_DS_DRAW_IDX_IND && r->ind) {
            plat_mem_read(r->ind, r->a, 12);   // count, instances, first index
        }
        if (r->kind != VK_DS_REPORT) r->a[3] = UINT32_MAX;
    }
    if (qpool) {   // runs of consecutive queries
        VkDevice dev = vk_ctx()->device;
        for (uint32_t i = 0; i < f->n;) {
            if (f->q[i] == UINT32_MAX) { i++; continue; }
            uint32_t j = i + 1;
            while (j < f->n && j - i < 256 && f->q[j] == f->q[j - 1] + 1 && f->q[j] < QN) j++;
            uint64_t v[2 * 256];
            VkResult res = vkGetQueryPoolResults(dev, qpool, f->q[i], j - i, sizeof v, v, 2 * sizeof v[0],
                                                 VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
            for (uint32_t k = i; k < j; k++) {
                uint64_t c = v[2 * (k - i)], av = v[2 * (k - i) + 1];
                f->r[k].a[3] = (res == VK_SUCCESS || res == VK_NOT_READY) && av ? (uint32_t)(c < UINT32_MAX - 1 ? c : UINT32_MAX - 2)
                                                                                 : UINT32_MAX - 1;
            }
            vkResetQueryPool(dev, qpool, f->q[i], j - i);
            for (uint32_t k = i; k < j; k++) __atomic_store_n(&q_busy[f->q[k]], 0, __ATOMIC_RELEASE);
            i = j;
        }
    }
    plat_mutex_lock(&q_lock);
    f->next = NULL;
    if (q_tail) q_tail->next = f;
    else q_head = f;
    q_tail = f;
    plat_cond_signal(&q_cv);
    plat_mutex_unlock(&q_lock);
}

static void open_ledger(void) {
    const char *d = cfg_get("NX2NAVI_DRAWSET");
    state = -1;
    if (!d || !*d) return;
    snprintf(dir, sizeof dir, "%s", d);
    plat_file_mkdir_p(dir);
    const char *r = cfg_get("NX2NAVI_DRAWSET_FRAMES");
    unsigned long long a, b;
    if (r && sscanf(r, "%llu-%llu", &a, &b) == 2) lo = a, hi = b;
    const char *x = cfg_get("NX2NAVI_DRAWSET_DROP");
    if (x && sscanf(x, "%llu:%127s", &a, drop_text) == 2) drop_frame = a;
    VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = 16ull * RING,
                             .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                                  .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                  .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    VmaAllocation al;
    VmaAllocationInfo info;
    if (vmaCreateBuffer(vk_ctx()->vma, &bi, &ai, &rb, &al, &info) == VK_SUCCESS) rb_map = info.pMappedData;
    else rb = VK_NULL_HANDLE;
    if (!cfg_get("NX2NAVI_DRAWSET_FRAGS") || cfg_bool("NX2NAVI_DRAWSET_FRAGS")) {   // =0: no fragment counts
        VkQueryPoolCreateInfo qi = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                    .queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS, .queryCount = QN,
                                    .pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT};
        if (vkCreateQueryPool(vk_ctx()->device, &qi, NULL, &qpool) == VK_SUCCESS) vkResetQueryPool(vk_ctx()->device, qpool, 0, QN);
        else qpool = VK_NULL_HANDLE;
    }
    if (!plat_thread_new(writer, NULL, "drawset")) {
        rt_log("drawset: no writer thread: off");
        return;
    }
    state = 1;
    rt_log("drawset: frames %llu-%llu to %s, %u-byte records, GPU arguments %s, fragment counts %s; drop: %s",
           (unsigned long long)lo, (unsigned long long)hi, dir, (unsigned)sizeof(DsRec), rb_map ? "read back" : "not read",
           qpool ? "on" : "off", drop_frame ? x : "none");
}

// Replay thread, at each present (vk_backend.c present_image, after vk_exec_frame_log): closes the frame replayed so
// far and opens the next.
void vk_drawset_frame(uint64_t frame) {
    if (!state) open_ledger();
    if (state < 0) return;
    if (fr) {
        fr->frame = cur, fr->gfx_frame = __atomic_load_n(&gfx_counters.frames, __ATOMIC_RELAXED);
        fr->tick = frame_step_ticks(), fr->world_us = frame_step_world_us();
        if (last_fence) vk_completion_add(last_fence, frame_done, fr);
        else frame_done(fr);
        fr = NULL;
    }
    (void)frame;
    cur++;
    pass_base = vk_exec_frame.passes;
    last_fence = 0;
    vk_drawset_on = cur >= lo && cur <= hi;
    if (vk_drawset_on) {
        fr = calloc(1, sizeof *fr);
        if (!fr) rt_fatal("drawset: out of memory");
    }
}

static DsRec *rec(VkExec *ex, int kind) {
    if (!fr) return NULL;
    if (fr->n == fr->cap) {
        fr->cap = fr->cap ? fr->cap * 2 : 4096;
        fr->r = realloc(fr->r, sizeof *fr->r * fr->cap);
        fr->q = realloc(fr->q, sizeof *fr->q * fr->cap);
        if (!fr->r || !fr->q) rt_fatal("drawset: out of memory");
    }
    DsRec *r = &fr->r[fr->n];
    memset(r, 0, sizeof *r);
    r->ord = fr->n++;
    r->kind = (uint8_t)kind;
    r->pass = (uint16_t)(vk_exec_frame.passes - pass_base);
    r->slot = UINT32_MAX;
    fr->q[r->ord] = UINT32_MAX;
    r->prog = ex->prog ? ex->prog->serial : 0;
    r->color = ex->rt.n > 0 && ex->rt.color[0] ? (uint64_t)(uintptr_t)ex->rt.color[0]->mem : 0;
    r->depth = ex->rt.depth ? (uint64_t)(uintptr_t)ex->rt.depth->mem : 0;
    if (ex->fence > last_fence) last_fence = ex->fence;
    return r;
}

static void stage_res(const VkExec *ex, DsRec *r, int mesh) {
    uint64_t h = 1469598103934665603ull;
    for (int s = mesh ? 6 : 0; s <= (mesh ? 7 : 0); s++)
        for (int i = 0; i < 18; i++) {
            uint64_t a = ex->ubo[s][i].addr;
            if (a) h = (h ^ a ^ (uint64_t)(s * 32 + i)) * 1099511628211ull;
        }
    r->res = h;
    r->place = ex->ubo[mesh ? 7 : 0][4].addr;
    r->tex = ex->tex[1][0];
    if (!mesh) r->vb = ex->vbuf[0].address;
}

static void vk_drawset_key(const DsRec *r, char *buf, size_t cap) {
    snprintf(buf, cap, "%u:%llu:%llx:%llx:%llx:%llx:%llx:%llx", r->kind, (unsigned long long)r->prog,
             (unsigned long long)r->color, (unsigned long long)r->depth, (unsigned long long)r->vb, (unsigned long long)r->ib,
             (unsigned long long)r->caller, (unsigned long long)r->place);
}

// A draw about to be recorded (its pipeline and resources bound). 1: drop it (NX2NAVI_DRAWSET_DROP).
int vk_drawset_draw(VkExec *ex, int kind, int prim, uint32_t count, uint32_t inst, int32_t first, uint32_t binst,
                    uint64_t ib, uint64_t ind, uint64_t caller, uint32_t slot) {
    DsRec *r = rec(ex, kind);
    if (!r) return 0;
    int mesh = kind == VK_DS_MESH || kind == VK_DS_MESH_IND;
    r->prim = (uint8_t)prim, r->count = count, r->inst = inst, r->first = first, r->binst = binst;
    r->ib = ib, r->ind = ind, r->caller = caller, r->slot = slot;
    if (kind != VK_DS_DISPATCH && kind != VK_DS_DISPATCH_IND) stage_res(ex, r, mesh);
    if (drop_frame && cur == drop_frame && !dropped) {
        char k[256], g[160];
        vk_drawset_key(r, k, sizeof k);
        snprintf(g, sizeof g, "%u:%llu:%llx:%llx:%llx", r->kind, (unsigned long long)r->prog, (unsigned long long)r->vb,
                 (unsigned long long)r->ib, (unsigned long long)r->caller);   // drawset.py's default key
        if (strstr(k, drop_text) || strstr(g, drop_text)) {
            dropped = 1;
            rt_log("drawset: frame %llu: dropped draw %u, key %s", (unsigned long long)cur, r->ord, k);
            fr->n--;   // not drawn, not in the ledger
            return 1;
        }
    }
    if (qpool && kind <= VK_DS_MESH_IND && ex->in_pass) {
        vk_drawset_draw_end(ex);
        uint32_t q = q_next % QN;
        if (!__atomic_load_n(&q_busy[q], __ATOMIC_ACQUIRE)) {
            q_busy[q] = 1, q_next++;
            vkCmdBeginQuery(ex->cb, qpool, q, 0);
            ex->recorded++;
            fr->q[r->ord] = q;
            q_open = 1;
        }
    }
    return 0;
}

// After the draw's command, and at every pass end (a query must end in its pass): ends the open fragment query.
void vk_drawset_draw_end(VkExec *ex) {
    if (!q_open) return;
    q_open = 0;
    if (fr && fr->n) vkCmdEndQuery(ex->cb, qpool, fr->q[fr->n - 1]);
    ex->recorded++;
}

// A draw prepare_draw refused (vk_exec_draw.c draw_skipped).
void vk_drawset_skip(VkExec *ex, int mesh, int why) {
    DsRec *r = rec(ex, mesh ? VK_DS_MESH : VK_DS_DRAW);
    if (!r) return;
    r->why = (uint32_t)why + 1;
    stage_res(ex, r, mesh);
}

// A 16-byte slot of the host-visible ring that the caller copies a draw's or dispatch's indirect arguments into
// (outside a pass); VK_NULL_HANDLE when the frame is not recorded.
VkBuffer vk_drawset_arg_slot(VkDeviceSize *off, uint32_t *slot) {
    *slot = UINT32_MAX;
    if (!vk_drawset_on || !fr || !rb) return VK_NULL_HANDLE;
    *slot = rb_next++;
    *off = 16ull * (*slot % RING);
    return rb;
}

// A samples-passed report to addr in the frame being replayed: the cell its count goes into (vk_drawset_report_value
// from the completion thread), UINT32_MAX when the frame is not recorded.
uint32_t vk_drawset_report(VkExec *ex, uint64_t addr) {
    DsRec *r = rec(ex, VK_DS_REPORT);
    if (!r) return UINT32_MAX;
    r->ind = addr;
    r->slot = cell_next++;
    cells[r->slot % RING] = UINT64_MAX;   // not written yet
    return r->slot;
}

void vk_drawset_report_value(uint32_t cell, uint64_t v) {
    if (cell != UINT32_MAX) cells[cell % RING] = v;
}
