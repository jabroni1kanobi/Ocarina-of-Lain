// NX2NAVI_FLIGHT=1 flight recorder (flight.c): the last frames' timing and event counts plus a thumbnail each, written to
// disk on F8 or after a slow frame, so a glitch or stutter seen in play can be read after the fact.
#pragma once
#include <stdint.h>

#define FLIGHT_THUMB_W 320
#define FLIGHT_THUMB_H 180

extern int flight_on;
extern int vk_dump_white;   // vk_dump.c, NX2NAVI_DUMP_WHITE   // NX2NAVI_FLIGHT=1, set once by flight_init before the first present

// NX2NAVI_FLIGHT_X=1 (diagnostic, with NX2NAVI_FLIGHT=1): per-frame time accumulators that any thread adds to and
// flight_frame takes and zeroes: the replay, the GPU fence waits of the render thread and of other threads, and the time
// the render thread and the guest main thread spend blocked in guest SVCs (rt_svc). They are the frames.tsv columns
// replay_ms, gpuwait_r_ms, gpuwait_o_ms, rsvc_ms, msvc_ms, submit_ms (vk_sync_submit after the recording: upload flush,
// prologue, vkQueueSubmit2). A wait counts whole in the frame it ends in.
// Inside the replay: bind_ms (bind_resources of every draw and dispatch, pool_ms included), pool_ms (pool_sync: the
// texture and sampler pool descriptors), rt_ms (resolve_rt: the pass's targets, their images created on first use),
// teximg_ms (texture image creation: VkImage + its memory, any thread).
// replay_mcyc: the CPU cycles the recording threads used (millions; against replay_ms: cycles per ms fall when the
// thread was off the CPU). cpu, cpu_moves: the logical processor of the render thread's last recording and how often it
// changed between recordings. pool_refills, pool_full: slots refilled and whole-table walks of the pool sync;
// pool_lock_ms: its wait for the texture lock; texlock_ms: the render thread's time taking the texture lock anywhere.
enum { FX_REPLAY, FX_GPUWAIT_R, FX_GPUWAIT_O, FX_RSVC, FX_MSVC, FX_SUBMIT, FX_BIND, FX_POOL, FX_RT, FX_TEXIMG,
       FX_REPLAY_CYC, FX_CPU, FX_CPUMOVE, FX_POOL_REFILL, FX_POOL_FULL, FX_POOL_LOCK, FX_TEXLOCK,
       // per frame, set by flight_frame itself (millions of cycles): the render thread's own CPU, and how busy the two
       // logical processors of its core were (elapsed cycles minus the idle thread's): busy beyond the render thread's
       // share is another thread on the reserved core
       FX_RTHR_CYC, FX_BUSY_EVEN, FX_BUSY_ODD, FX_ELAPSED_CYC,
       FX_GUARD_CYC,   // the sibling guard's own cycles (NX2NAVI_REPLAY_GUARD): part of the core's busy time, not foreign
       // Who records and where a recording's time goes outside the draws (vk_sync_submit, vk_exec_record): rec_main_ms /
       // rec_other_ms the wall time of recordings made by the guest main thread / by any thread that is neither it nor
       // the render thread (the present thread's empty one included); sublock_ms the render thread's wait for the
       // submission lock (another thread recording); pins_ms nvn_pins_sweep; usnap_ms the uniform snapshot (begin + end);
       // mirror_ms the VRAM mirror (begin + end, the write-watch snapshot and the freeze pass in it); freeze_ms the
       // freeze pass alone; xfer_ms the copy / clear / event ops. thaw_ms: write-fault thaws of frozen regions, any
       // thread (memory.c). n_freeze, n_thaw: regions frozen and thawed in the frame.
       FX_REC_MAIN, FX_REC_OTHER, FX_SUBLOCK, FX_PINS, FX_USNAP, FX_MIRROR, FX_FREEZE, FX_XFER, FX_THAW, FX_NFREEZE, FX_NTHAW,
       // gen_slots, gen_samp, gen_entry: how far vk_texture_slots_gen, vk_sampler_gen and vk_texture_entry_gen moved
       // in the frame (what flushes the per-draw memos). probe_cpu, probe_thr, probe_mem: thousands of cycles three
       // fixed pieces of work took at the frame's first recording on the render thread (flight_x_probe: one dependent
       // chain = the core's clock; twelve independent chains = the clock and a busy sibling logical processor; a
       // pointer walk over a private buffer = caches and TLB): the same work running slower shows there, more work
       // does not.
       // page_faults: the process's page faults in the frame. dev_allocs: vkAllocateMemory calls (any thread).
       // rtpool: images that took memory from the render-target pools (vk_texture.c rt_alloc).
       FX_GEN_SLOTS, FX_GEN_SAMP, FX_GEN_ENTRY, FX_PROBE_CPU, FX_PROBE_THR, FX_PROBE_MEM, FX_PAGE_FAULTS, FX_DEV_ALLOCS, FX_RTPOOL,
       FX_N };
extern int flight_x_on;
extern volatile int64_t flight_x_ns[FX_N];
extern volatile uint64_t flight_x_frame;   // the frame flight_frame recorded last (log lines of NX2NAVI_FLIGHT_X name it)
#define FX_T0() (flight_x_on ? kern_now_ns() : 0)
#define FX_ADD(kind, t0) do { if (t0) plat_add64(&flight_x_ns[kind], (int64_t)(kern_now_ns() - (t0))); } while (0)
#define FX_COUNT(kind, n) do { if (flight_x_on) plat_add64(&flight_x_ns[kind], (int64_t)(n) * 1000000); } while (0)
// One recording's stages in ns (vk_exec_record fills it under vk_sync_submit's lock, vk_sync_submit reads and logs it).
typedef struct { uint64_t pins, usnap, mirror, walk, xfer, xfer_max; int xfer_max_op, xfer_n, handles; } FlightXRec;
extern FlightXRec flight_x_rec;
void flight_x_probe(void);   // the two speed probes, once per frame (the render thread's first recording)
void flight_x_svc(uint64_t t0, uint64_t t1);   // rt_svc: a blocking SVC of the calling thread ran from t0 to t1

void flight_init(void);   // prof_start calls it
// perf_present, once per present on the render thread: the interval since the previous present, the render thread's
// blocked time per PERF_* kind in this frame, and the running PERF_CNT_* totals.
void flight_frame(uint64_t frame, uint64_t now_ns, uint64_t interval_ns, const uint64_t blocked_ns[],
                  const volatile int64_t counts[]);

// gfx_present.c: the present thread blits every presented frame into a host-visible ring of n thumbnails.
void vk_thumbs_enable(int n);            // before the first present; n slots of FLIGHT_THUMB_W x FLIGHT_THUMB_H
void vk_thumbs_frame(uint64_t frame);    // the present thread names the frame of the vk_present_image call that follows
// Copies frame's thumbnail as RGBA when the ring still holds it complete; 0 when it was never written, is still in
// flight or was overwritten. Takes the present lock for one slot copy (about 20 us); any thread.
int vk_thumbs_read(uint64_t frame, uint8_t *rgba);
