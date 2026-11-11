// --probe-gfx-abi: every host object of nvn_obj.h fits the storage nvn.h reserves
// for its NVN type, the layouts the game reads directly (NVNtextureView, the command buffer's inline fast-path
// fields) sit where NX2NAVI's NVN 55 code expects them, and the GPU address window lies above every guest region.
// The same facts are static assertions in nvn_obj.h; this mode prints the sizes so a change shows its margin.
// Needs no game files.
#include <stddef.h>
#include <stdio.h>

#include "gfx.h"
#include "platform/platform.h"

static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("  FAIL: " __VA_ARGS__); putchar('\n'); } } while (0)

typedef struct { const char *host, *nvn; size_t size, reserved; } Fit;
#define FIT(T, H) {#T, #H, sizeof(T), sizeof(H)}
static const Fit fits[] = {
    FIT(dev_builder_t, NVNdeviceBuilder),       FIT(dev_t_, NVNdevice),
    FIT(queue_builder_t, NVNqueueBuilder),      FIT(queue_t, NVNqueue),
    FIT(pool_builder_t, NVNmemoryPoolBuilder),  FIT(pool_t, NVNmemoryPool),
    FIT(dpool_t, NVNtexturePool),               FIT(dpool_t, NVNsamplerPool),
    FIT(tex_builder_t, NVNtextureBuilder),      FIT(tex_t, NVNtexture),
    FIT(view_t, NVNtextureView),
    FIT(samp_builder_t, NVNsamplerBuilder),     FIT(samp_t, NVNsampler),
    FIT(prog_t, NVNprogram),
    FIT(sync_t, NVNsync),
    FIT(ev_builder_t, NVNeventBuilder),         FIT(ev_t, NVNevent),
    FIT(win_builder_t, NVNwindowBuilder),       FIT(win_t, NVNwindow),
    FIT(cmd_t, NVNcommandBuffer),
    FIT(color_st, NVNcolorState),               FIT(chmask_st, NVNchannelMaskState),
    FIT(blend_st, NVNblendState),               FIT(ms_st, NVNmultisampleState),
    FIT(poly_st, NVNpolygonState),              FIT(ds_st, NVNdepthStencilState),
    FIT(va_st, NVNvertexAttribState),           FIT(vs_st, NVNvertexStreamState),
};

_Static_assert(sizeof(NVNtextureView) == 16, "NVNtextureView is 16 bytes in NX2NAVI's NVN 55");
_Static_assert(offsetof(cmd_t, inline_cur) == 0x20 && offsetof(cmd_t, inline_end) == 0x28, "cmd_t inline fields");
_Static_assert(RT_GPU_VA_BASE >= RT_ADDRESS_LIMIT, "GPU address window overlaps the guest address space");

// Write watch: a committed heap page and an alias page, written once, are reported once.
static void check_write_watch(void) {
    const RtGuestLayout *g = mem_layout();
    uint64_t heap = 0, pages[8], n;
    CHECK(mem_set_heap_size(0x200000, &heap) == 0 && heap == g->heap.base, "SetHeapSize 2 MiB");
    mem_written_pages(heap, 0x200000, pages, 8);   // drop whatever the commit itself marked
    volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)heap;
    p[0x5000] = 1;
    n = mem_written_pages(heap, 0x200000, pages, 8);
    CHECK(n == 1 && pages[0] == heap + 0x5000, "heap write: %llu pages reported (want 1 at %#llx)",
          (unsigned long long)n, (unsigned long long)(heap + 0x5000));
    printf("write-watch: heap page written once: %llu reported at +%#llx\n", (unsigned long long)n,
           n ? (unsigned long long)(pages[0] - heap) : 0ull);
    n = mem_written_pages(heap, 0x200000, pages, 8);
    CHECK(n == 0, "second call reported %llu pages (want 0)", (unsigned long long)n);
    printf("write-watch: second call: %llu reported\n", (unsigned long long)n);
    p[0x7000] = 2;
    p[0x9000] = 3;
    p[0xA000] = 4;
    n = mem_written_pages(heap, 0x200000, pages, 2);
    uint64_t m = mem_written_pages(heap, 0x200000, pages + 2, 6);
    printf("write-watch: three pages written, max 2: %llu then %llu\n", (unsigned long long)n, (unsigned long long)m);
    CHECK(n == 2 && m == 1, "max 2 reported %llu, then %llu (want 2, 1)", (unsigned long long)n, (unsigned long long)m);

    // Frozen region: read-only until the first write thaws it (the write lands and is reported); mem_thaw (a file
    // read) thaws it and holds it thawed; SetHeapSize below decommits it.
    mem_written_pages(heap, 0x200000, pages, 8);
    CHECK(mem_freeze(heap) == 1 && mem_frozen(heap), "freeze of an idle committed 2 MiB region");
    uint64_t end;
    CHECK(plat_mem_state(heap, heap + 1, &end) == PLAT_PROT_READ, "frozen region is not read-only");
    p[0x3000] = 7;
    CHECK(!mem_frozen(heap) && p[0x3000] == 7, "a write into a frozen region did not thaw it");
    n = mem_written_pages(heap, 0x200000, pages, 8);
    CHECK(n == 1 && pages[0] == heap + 0x3000, "write after a thaw: %llu pages reported (want 1)", (unsigned long long)n);
    CHECK(mem_freeze(heap) == 1, "refreeze after a write thaw");
    mem_thaw(heap + 0x6000, 0x1000);
    CHECK(!mem_frozen(heap) && mem_freeze(heap) == 0, "mem_thaw did not thaw and hold the region");
    p[0x6000] = 8;
    printf("write-watch: frozen region thawed by a write (reported once) and by mem_thaw (held)\n");

    uint64_t a = g->alias.base + 0x10000;
    CHECK(mem_map_physical(a, 0x4000) == 0, "MapPhysicalMemory");
    mem_written_pages(a, 0x4000, pages, 8);
    *(volatile uint8_t *)(uintptr_t)(a + 0x1000) = 1;
    n = mem_written_pages(a, 0x4000, pages, 8);
    CHECK(n == 1 && pages[0] == a + 0x1000, "alias write: %llu pages reported (want 1)", (unsigned long long)n);
    CHECK(mem_written_pages(a, 0x4000, pages, 8) == 0, "alias: second call reported pages");

    // A region with reserved holes freezes its committed pages only, and a write thaws them without committing a hole.
    uint64_t ra = a & ~0x1fffffull;
    CHECK(mem_freeze(ra) == 1 && mem_frozen(ra), "freeze of a 2 MiB region with reserved holes");
    CHECK(plat_mem_state(a, a + 1, &end) == PLAT_PROT_READ, "its committed pages are not read-only");
    *(volatile uint8_t *)(uintptr_t)(a + 0x2000) = 9;
    CHECK(!mem_frozen(ra) && *(volatile uint8_t *)(uintptr_t)(a + 0x2000) == 9, "a write did not thaw the region with holes");
    CHECK(plat_mem_state(a, a + 0x10000, &end) == PLAT_PROT_RW && end == a + 0x4000, "the thawed pages are not read-write");
    CHECK(plat_mem_state(a + 0x4000, a + 0x4001, &end) == PLAT_MEM_RESERVED, "a hole got committed");
    printf("write-watch: region with reserved holes frozen and thawed by a write\n");
    // A mapping change thaws first: unmapping a frozen region leaves it thawed and reserved, with nothing to freeze.
    CHECK(mem_freeze(ra) == 1, "refreeze of the region with holes");
    CHECK(mem_unmap_physical(a, 0x4000) == 0, "UnmapPhysicalMemory");
    CHECK(!mem_frozen(ra) && plat_mem_state(a, a + 0x4000, &end) == PLAT_MEM_RESERVED && end == a + 0x4000,
          "unmapping a frozen region did not thaw and release it");
    CHECK(mem_freeze(ra) == 0, "a region without a committed page froze");
    CHECK(mem_written_pages(RT_KERNEL_BASE, 0x1000, pages, 8) == UINT64_MAX, "kernel region is not write-watched");
    printf("write-watch: alias page written once: reported once, then not\n");

    // A moved stack (MapMemory): its source has no access while moved, so the source's region does not freeze; the
    // destination freezes like any committed memory, and the move back thaws it before it is released.
    CHECK(mem_set_heap_size(0x400000, &heap) == 0, "SetHeapSize 4 MiB");
    uint64_t h2 = heap + 0x200000, src = h2 + 0x3000, dst = g->stack.base + 0x10000, rd = dst & ~0x1fffffull;
    CHECK(mem_map(dst, src, 0x2000) == 0, "MapMemory of two heap pages into the stack region");
    CHECK(mem_freeze(h2) == 0 && !mem_frozen(h2), "a region with a no-access page froze");
    CHECK(mem_freeze(rd) == 1, "freeze of a moved stack's region");
    *(volatile uint8_t *)(uintptr_t)(dst + 0x1000) = 5;
    CHECK(!mem_frozen(rd) && mem_freeze(rd) == 1, "a write did not thaw the moved stack, or it did not freeze again");
    CHECK(mem_unmap(dst, src, 0x2000) == 0, "UnmapMemory");
    CHECK(!mem_frozen(rd) && *(volatile uint8_t *)(uintptr_t)(src + 0x1000) == 5, "the move back lost the stack's bytes");
    CHECK(plat_mem_state(dst, dst + 1, &end) == PLAT_MEM_RESERVED, "the moved stack's pages stayed committed");
    CHECK(mem_freeze(h2) == 1 && mem_freeze(rd) == 0, "after the move back: the source's region must freeze, the empty one not");
    printf("write-watch: moved stack: source region refused while moved, destination frozen and thawed\n");

    // A freeze costs the same whatever lies behind the region. The system's own query sizes the whole run of
    // same-state pages from the address on: 8-9 ms with 1 GiB of written memory behind the region, the long frame each
    // time a streamed texture's region froze again. NX2NAVI_FREEZE_CHECK=1 asks the system as well and shows that cost.
    const uint64_t big = 0x40000000ull;
    CHECK(mem_set_heap_size(0x400000 + big, &heap) == 0, "SetHeapSize 1 GiB + 4 MiB");
    for (uint64_t o = 0x200000; o < 0x400000 + big; o += 0x1000) p[o] = 1;   // h2 thaws; every page behind it is written
    CHECK(!mem_frozen(h2), "a write did not thaw the region");
    uint64_t best = UINT64_MAX;
    for (int k = 0; k < 5; k++) {
        uint64_t t0 = plat_now_ns();
        int ok = mem_freeze(h2);
        uint64_t t1 = plat_now_ns();
        CHECK(ok == 1, "freeze of a region with 1 GiB of written memory behind it");
        if (t1 - t0 < best) best = t1 - t0;
        p[0x200000 + 0x1000 * (uint64_t)k] = 2;
        CHECK(!mem_frozen(h2), "a write did not thaw the region");
    }
    printf("write-watch: freeze of a 2 MiB region with 1 GiB of written memory behind it: %.3f ms (best of 5)%s\n",
           (double)best / 1e6, plat_mem_check ? ", the system asked as well (NX2NAVI_FREEZE_CHECK)" : "");
    if (!plat_mem_check) CHECK(best < 1000000, "that freeze took %.3f ms (want under 1 ms)", (double)best / 1e6);
    CHECK(mem_set_heap_size(0, &heap) == 0, "SetHeapSize 0");
    if (plat_mem_check) {
        printf("write-watch: %lld page-state answers compared with the system's, %lld differed\n",
               (long long)plat_mem_checked, (long long)plat_mem_differed);
        CHECK(plat_mem_checked > 0 && !plat_mem_differed, "the platform's page record differs from the system");
    }
}

int probe_gfx_abi(void) {
    size_t tightest = (size_t)-1;
    const char *tight = "";
    for (size_t i = 0; i < sizeof fits / sizeof *fits; i++) {
        const Fit *f = &fits[i];
        CHECK(f->size <= f->reserved, "%s is %zu bytes, %s reserves %zu", f->host, f->size, f->nvn, f->reserved);
        if (f->reserved - f->size < tightest) tightest = f->reserved - f->size, tight = f->host;
    }
    CHECK(sizeof(NVNtextureView) == 16 && sizeof(view_t) == 16, "NVNtextureView %zu bytes, view_t %zu (want 16)",
          sizeof(NVNtextureView), sizeof(view_t));
    CHECK(offsetof(cmd_t, guest_flags) == 0x10, "cmd_t.guest_flags at %#zx (want 0x10)", offsetof(cmd_t, guest_flags));
    CHECK(offsetof(cmd_t, inline_cur) == 0x20, "cmd_t.inline_cur at %#zx (want 0x20)", offsetof(cmd_t, inline_cur));
    CHECK(offsetof(cmd_t, inline_end) == 0x28, "cmd_t.inline_end at %#zx (want 0x28)", offsetof(cmd_t, inline_end));
    CHECK(sizeof(samp_par) == 44, "samp_par is %zu bytes (want 44)", sizeof(samp_par));
    CHECK(RT_GPU_VA_BASE >= RT_ADDRESS_LIMIT && RT_GPU_VA_SIZE > 0, "GPU address window %#llx overlaps the guest space",
          RT_GPU_VA_BASE);
    check_write_watch();
    printf("gfx-abi: %d layouts fit (tightest %s, %zu bytes spare), cmd_t inline fields at +0x20/+0x28, "
           "NVNtextureView %zu bytes\n", (int)(sizeof fits / sizeof *fits), tight, tightest, sizeof(NVNtextureView));
    if (failures) {
        printf("probe-gfx-abi: FAILED (%d of %d checks)\n", failures, checks);
        return 1;
    }
    printf("probe-gfx-abi: passed\n");
    return 0;
}
