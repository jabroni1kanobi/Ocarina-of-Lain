// NX2NAVI_WAITLOG=<ms>: what the game's threads wait on in slow frames. rt_svc (svc.c) times every blocking SVC
// (SleepThread, WaitSynchronization, ArbitrateLock, WaitProcessWideKeyAtomic, WaitForAddress, SendSyncRequest*) and
// hands it here with the thread, the SVC's x0/x1 (handle, lock or address) and the guest call chain (x30, then up to
// three frame-pointer links). Only the part of a wait inside the current frame counts (a 100 ms sleep that ends in a
// frame adds that frame's share, not 100 ms). The waits of a frame are summed per (thread, SVC, chain) and per thread;
// at each present a frame longer
// than <ms> logs its eight longest groups ("waitlog: frame N ..."), and every NX2NAVI_WAITLOG_EVERY (600) frames one
// normal frame is logged the same way for comparison. NX2NAVI_WAITLOG_AFTER=<frame>: nothing logged before it.
// Off (unset), the only cost is one branch in rt_svc and perf_present.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfg.h"
#include "host.h"
#include "kernel.h"
#include "platform/platform.h"

#define WL_N 1024    // groups per frame
#define WL_DEPTH 4

typedef struct {
    uint64_t tid, chain[WL_DEPTH], a0, a1, ns, max_ns;
    uint32_t imm, n;
} WlGroup;

int waitlog_on;
static uint64_t slow_ns, after, every = 600;
static PlatMutex lock = PLAT_MUTEX_INIT;
static WlGroup g[WL_N];
static int ng, dropped;
static uint64_t frame_t0;   // when the current frame began (the last present)
#define WL_T 64
static struct { uint64_t tid, ns; } th[WL_T];
static int nth;

void waitlog_init(void) {
    const char *e = cfg_get("NX2NAVI_WAITLOG");
    if (!e || !*e) return;
    slow_ns = (uint64_t)(strtod(e, NULL) * 1e6);
    const char *a = cfg_get("NX2NAVI_WAITLOG_AFTER");
    if (a && *a) after = strtoull(a, NULL, 0);
    const char *v = cfg_get("NX2NAVI_WAITLOG_EVERY");
    if (v && *v) every = strtoull(v, NULL, 0);
    waitlog_on = 1;
    rt_log("waitlog: on: frames over %.1f ms list their longest guest waits (after frame %llu, a normal frame every %llu)",
           slow_ns / 1e6, (unsigned long long)after, (unsigned long long)every);
}

int waitlog_blocking(uint32_t imm) {
    return imm == 0x0B || imm == 0x18 || imm == 0x1A || imm == 0x1C || imm == 0x34 || imm == 0x21 || imm == 0x22;
}

void waitlog_svc(const Cpu *c, uint32_t imm, uint64_t a0, uint64_t a1, uint64_t t0, uint64_t t1) {
    uint64_t chain[WL_DEPTH] = {c->x[30]};
    uint64_t fp = c->x[29];
    for (int i = 1; i < WL_DEPTH && fp && !(fp & 7); i++) {
        uint64_t rec[2];
        if (!plat_mem_read(fp, rec, sizeof rec)) break;
        chain[i] = rec[1];
        if (rec[0] <= fp) break;   // frames grow up the stack
        fp = rec[0];
    }
    uint64_t tid = kern_thread_gid(c->thread);
    plat_mutex_lock(&lock);
    uint64_t from = t0 > frame_t0 ? t0 : frame_t0, ns = t1 > from ? t1 - from : 0;
    if (!ns) {
        plat_mutex_unlock(&lock);
        return;
    }
    int j = 0;
    while (j < nth && th[j].tid != tid) j++;
    if (j == nth && nth < WL_T) th[nth].tid = tid, th[nth].ns = 0, nth++;
    if (j < nth) th[j].ns += ns;
    int k = 0;
    for (; k < ng; k++)
        if (g[k].tid == tid && g[k].imm == imm && !memcmp(g[k].chain, chain, sizeof chain)) break;
    if (k == ng) {
        if (ng == WL_N) {
            dropped++;
            plat_mutex_unlock(&lock);
            return;
        }
        memset(&g[ng], 0, sizeof g[ng]);
        g[ng].tid = tid, g[ng].imm = imm, g[ng].a0 = a0, g[ng].a1 = a1;
        memcpy(g[ng].chain, chain, sizeof chain);
        ng++;
    }
    g[k].ns += ns, g[k].n++;
    if (ns > g[k].max_ns) g[k].max_ns = ns, g[k].a0 = a0, g[k].a1 = a1;
    plat_mutex_unlock(&lock);
}

static int by_ns(const void *a, const void *b) {
    const WlGroup *x = a, *y = b;
    return x->ns < y->ns ? 1 : x->ns > y->ns ? -1 : 0;
}

void waitlog_frame(uint64_t frame, uint64_t interval_ns) {
    plat_mutex_lock(&lock);
    uint64_t now = kern_now_ns();
    int slow = interval_ns > slow_ns, normal = every && frame % every == 0;
    if (frame >= after && (slow || normal) && ng) {
        WlGroup *s = malloc(sizeof *s * (size_t)ng);
        if (s) {
            memcpy(s, g, sizeof *s * (size_t)ng);
            int n = ng;
            qsort(s, (size_t)n, sizeof *s, by_ns);
            uint64_t tick = frame_step_ticks();
            rt_log("waitlog: frame %llu tick %llu %s %.2f ms: %d wait groups%s", (unsigned long long)frame,
                   (unsigned long long)tick, slow ? "SLOW" : "normal", interval_ns / 1e6, n, dropped ? " (some dropped)" : "");
            char tl[1024];
            int k = 0;
            for (int i = 0; i < nth && k < (int)sizeof tl - 40; i++)
                if (th[i].ns >= 1000000) k += snprintf(tl + k, sizeof tl - k, " t%llu %.1f", (unsigned long long)th[i].tid, th[i].ns / 1e6);
            tl[k] = 0;
            rt_log("waitlog:   threads waiting over 1 ms (ms):%s", tl);
            for (int i = 0; i < n && i < 8; i++)
                rt_log("waitlog:   t%llu svc %#x x%u total %.2f ms max %.2f ms x0 %#llx x1 %#llx chain %#llx %#llx %#llx %#llx",
                       (unsigned long long)s[i].tid, s[i].imm, s[i].n, s[i].ns / 1e6, s[i].max_ns / 1e6,
                       (unsigned long long)s[i].a0, (unsigned long long)s[i].a1, (unsigned long long)s[i].chain[0],
                       (unsigned long long)s[i].chain[1], (unsigned long long)s[i].chain[2],
                       (unsigned long long)s[i].chain[3]);
            free(s);
        }
    }
    ng = 0, dropped = 0, nth = 0;
    frame_t0 = now;
    plat_mutex_unlock(&lock);
}
