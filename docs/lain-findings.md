# Ocarina of Lain on RX 9070: findings (session started 2026-11-08T06:46Z; stamps from date -u)

## RGP: unusable on this driver and game (Observed, 4 of 4 runs)
Tool: Radeon Developer Tool Suite 2026-10-02-1872 (RGP 2.7.1), RadeonDeveloperPanelCLI, driver 26.10.1.
With the panel CLI attached (profiling or crash mode) the game loses its Vulkan device on entering an outdoor scene,
before any capture was requested:
- runs/rgp-kok1, rgp-kok2 (Kokiri route), rgd-kok1: pass 472 of the submission, 1920x1080, vs bcd47dba4e40ae21 fs eaffb395f1574b12.
- runs/rgp-deku1 (new game route): device lost at the scene load near frame 10980.
- Without the CLI the same route runs (runs/base-kok1). So the attach triggers it; commit was fine in rgp-kok2 (62.6 of 70.6 GB).
- RGD (runs/rgd-kok1/crash.txt): GPU hang in DispatchMeshIndirectMulti (the 5th of a run after a Dispatch 18x1x1), no page fault.
  Not chased further: it does not happen in normal play.
Fallback per the handoff: per-pass GPU timestamps in the source build.

## Source build (Confirmed builds; picture and GPU parity with the install NOT yet shown)
- Installed kit runtime == source 0.5.0 runtime (0 of the .c/.h/.cpp/.inc files differ).
- nx2navi-src\build_rt.py compiles runtime/ with the launcher's clang and relinks against the install's game objects,
  read-only: 13 s, 5124 KB of runtime objects vs 5125 KB in the install. Nothing is written into the install except
  loose shader translations through the out\shader-cache junction, same translator identity.
- src commit 297f1b1 adds NX2NAVI_PASSTIME=<frame>[:<frames>] + NX2NAVI_PASSTIME_OUT; off unless set. pt_summary.py reads the table.

## GPU averages seen so far (not yet comparable)
- base-kok1 (install, no flight): Kokiri load 41-46 ms for 2 windows, then 27 ms, later 6.6 ms, later 16 ms.
- src-kok1 (vanilla source, flight on): 42.9, then 33-34 ms. One variable differs (flight), and frame numbers shift between
  runs, so this is not a parity result yet.

## Where the GPU time goes (Confirmed by runs/lock-off, camera locked with NX2NAVI_INPUT=none, 2026-11-08T08:05Z)
Runs before lock-off could be moved by the owner's input (he did move the camera at least once): ignore their absolute
numbers, and the 8 ms "gain" seen in pt-x4/ab-on, which was a later camera, not a fix. The window view after a load costs
24 ms GPU, then the game's own camera swings (with input off too) and it drops to about 8 ms.
Heavy view, per frame: span 33.8 ms = passes 20.4 + dispatches 1.5 + between marks 11.9.
1. 1600x1600 depth-only shadow passes, mesh 45b842be + alpha-test fs a59479bf: 14.9 ms (3 passes; 91 groups, 11,576 prims,
   901 M fragment invocations in one pass, about 78,000 per prim, 350x overdraw). 1,679 M fragments a frame in all.
2. Same pass group with mesh 9cb4c068 + fs 44a9a66d: 2.3 ms.
3. All 1920x1080 scene passes: about 2 ms. Dispatches: 1.5 ms.
4. Between marks: two gaps of 6-8 ms per frame at submission boundaries (likely GPU idle: 33.8 span vs 24 busy), 2.5-3 ms
   between 1080p passes P29 and P30 (not identified), small barrier gaps total only about 2 ms over 570 marks: full
   barriers are NOT a main cost (Rejected as a lead for now). Render-pass breaks for mesh draws: not visible either.
Fill-bound: NX2NAVI_SHADOW_SCALE=2 raised item 1 by 3.9x. NX2NAVI_SHADOW_RES has no effect on game 1.1.1 (no patch sets).
Rejected: REQUIRE_FULL_SUBGROUPS on mesh stages: 2,627 M fragments and 33 ms against 1,679 M and 24 ms (lock-on vs lock-off).
It does show the output depends on the wave layout. Flight recorder on adds about 5 ms GPU: keep it off when timing.
Note: pipeline statistics come back in bit order, so the table's `prims` column is FS invocations and `frags` is clipped prims.

## ROOT CAUSE of the wide-view GPU cost (Confirmed, 2026-11-08T08:30Z)
The translator's registers are float variables, so game integers become float constants with those bits: 1 = denormal
0x1, -1 = NaN 0xFFFFFFFF. Maxwell moves them untouched; AMD's default float mode flushes the denormal and quiets the NaN.
In mesh 45b842be / 9cb4c068 (leaf sprites) that is the +1/-1 sign of the sprite scale (L1777 select, L1878 bitcast, L1879
ConvertSToF), which became about 2e9 or 0: triangles over the whole shadow map. Those two are the only ones of the mesh
shaders compared that carry such constants; 3,601 of 36,678 modules do (ps 2,715, vs 736, cs 100, ms 50).
Proof, camera locked, same groups (52, 8, 93, 18): 901 M fragments -> 29 k in the largest pass, 1,679 M -> 35 M a frame.
- e1: hand edit of the 2 shaders to an integer. e2: DenormPreserve + SignedZeroInfNanPreserve on the 2 shaders: same result.
- Fix: gfx_shader.c spirv_float_bits (load time, in memory, no retranslation; NX2NAVI_FLOAT_BITS=0 disables).
Results, RX 9070, 2560x1440:
                         GPU before  GPU after  interval (submit_finish 1)  interval (submit_finish 0)
Kokiri heavy view          24 ms       8.5-11     28 ms                       17.8 ms (56 fps)
Deku Tree cutscene         31 ms       7.5-8      20 ms                       16.67 ms (60 fps)
Kokiri's remaining cost is CPU: replay 14.6 ms (mirror uploads 82-109 MiB a frame, 420 syncs).
Picture: changes. Before = flat, dim, ground shadowed by the junk triangles. After = sun haze and light shafts. The old
OoT recomp's "haze" complaint is related: judge full-size frames with the owner, do not assume.
Flash meter trap: flashes.py compares across dump gaps (11431 was a boundary, not a flash). Use one continuous dump.
Deku flash gate, 1,500 continuous frames 10831-12330, submit_finish 0: candidate 0, control (NX2NAVI_FLOAT_BITS=0) 1. The control
also shows the pale white-out fog over the cutscene (cmp_deku.png: left fix, right control).
Soak (runs/soak1, source build, 670 s to frame 39000, kokiri_white): no device loss, interval mean 16.76 ms, 15 of 595 windows
over 17.5 ms (worst 28.5), GPU mean 7.0, max 13.2. Voices (runs/voice-cand): mod serves 1040 files, cutscene RMS 0.05-0.17.

## CORRECTION 2026-11-08T14:55Z: earlier flash claims were wrong
The owner saw flashes the meter called 0. flashes.py compares with a 31-frame median, so an event 31 frames long hides inside it.
flash3.py (per-pixel one-frame spikes) on Kokiri:
- float bits on all stages + submit_finish 0x1: frames 3272-3302 are black and white noise over the whole picture.
- same with NX2NAVI_FLOAT_BITS=0: 0 spike runs in 1,949 frames. So the all-stage fix caused it.
- mesh and task only (src d8ba170), 0x1: 0 spike runs in 1,586 frames; submit_finish 0: 117 runs, up to 59% of pixels. Keep 0x1.
- Deku, mesh only, 0x1: 0 spike runs in 1,500 frames, mean brightness as the all-stage fix (45/69/74/78): the white-out cure came
  from the shadows. 16.72 ms, GPU 7.5.
submit_finish: 0x1 = wait after a frame's first submission (default when unset), 1 = after every one, 0 = never.
Speed work: AVX2 game code (the 0.5.0 kit compiled it generic, every FMA a software call) won 3 of 3 pairs, interval 20.07 -> 19.12,
worst frame 34.5 -> 23.5. Runtime profile of the render thread in Kokiri: 26% blocked on another guest thread, bind_resources 32%,
amdvlk 15%, memcpy small. NX2NAVI_VK_RECORDER rejected unrun: its own comment reports lost devices on RDNA 4.
Still untested: ONE_PER_CORE, REPLAY_ASYNC, runtime at AVX2 (one sample only, ab3). Runs are noisy: compare alternating medians only.

## Edge bars ("box"), open, 2026-11-08T16:50Z
Symptom: pale stair-stepped patches near the screen edges, come and go over seconds (frames 9600-10800 have them, 11100-12000 mostly
not, Saria scene), and change with the camera. Steps of about 15 px at 1080p. Black side bars in test windows are separate: the window
is 2560x1410, so the 16:9 picture is 2508 wide; fullscreen should not have them (not yet checked by me in fullscreen).
Observed in: pass 224 img2 (1080p, the game's own picture), so before FSR and present. Pass 179 t1 (960x540 rgba16f haze) has them too.
Rejected: FSR/present; float bits on VS, FS or CS (masks 3, 5, 9 all still show it, none brings noise back); fog volume slices
(pass 79 images smooth); float blend support (fmtq.exe: all formats blend); mesh `first` (always 0 in 100 draws, one view);
the 480x270 polygon in passes 139-142 (left only, the right strip has no counterpart); skipping fs 1aba5cda2b65417f (pass 179 writes
no colour then, the patch is still on screen in shots), so pass 179 shares the cause rather than being it.
Later the same day, also rejected: depth-map border colour (NX2NAVI_BORDER_TEST); AO buffer (passes 71-75 clean); skipping fs bc280fb2a7cc7e22
(the pass where the wedge first shows); skipping fog injection dbe3ccba173ffb2d + 67ebcf9b12b90fbe: the box stays and is CONSTANT and clearer, an
inset about 5% per side, so fog only partly hides it. Thread pinning ONE_PER_CORE=1: about 105 ms a frame, rejected.
Harness bug fixed: -Extra env leaked into later runs of a batch, which made the "100 ms" batches (not the visible window).
This is a forward renderer: passes 182, 184, 195, 196 draw lit geometry. Pass 195 looks clean, 198 on has the box; pass 197 fs 3bdeddefe0d8de4b composites haze and reads the 480x270 rgba32f wedge. Being tested: skipping it.
Older lead, unproven: c0+0x510/0x520 storage buffers, bindings 8-13 (light lists); a Sonnet read found no cause there (shaders\edge-bars-analysis.md).
Async replay A/B never ran clean (the batch died on a closing game).
Tools: NX2NAVI_DUMP_ONLY, _EVERY, _FROM; pass numbers shift between frames and with skips, so pick stages by target size.
NX2NAVI_SKIP_SHADERS costs replay 14 -> 36 ms: such runs are slow, give them 520 s.
Deleting Downloads\Bloodborne-Windows (owner's OK, on my 29.9 GB figure) freed only about 0.5 GB; the figure was wrong.

## INSTALLED 2026-11-08 (exe from src commit 437da62, ini submit_finish = 0): SUPERSEDED, has the noise burst, ini now 0x1
Later on 2026-11-08: installed exe = d8ba170 (mesh-only float bits, upstream 42ae87bc, AVX2 game code), ini submit_finish 0x1. Backups as below.
Installed game, owner's ini, camera locked: Kokiri heavy view interval 16.7 ms, GPU 8; Deku cutscene 16.67 ms, GPU 7.5; 0 flashes.
Rollback: copy game\ocarina.exe.bak-0.5.0-original over ocarina.exe and nx2navi.ini.bak-before-floatfix over nx2navi.ini.
Not covered: tests run windowed (2560x1410), fullscreen is 2560x1440; Kokiri not flash-checked on one continuous dump;
commit free reaches 0.1-0.4 GB in Kokiri; the rest of the game is unmeasured. Next if needed: Kokiri CPU replay 14.6 ms.

## Disk and commit: a test run breaks the floor (Observed; owner allowed dips to about 24 GB, test runs only, 2026-11-08)
Base commit about 45-46 GB of a 68.9 GB limit; the game adds about 24 GB private (VRAM included) and pushes commit free to
0.3-0.4 GB, and the system-managed page file then jumps about 19 GB at once at the Kokiri load: C: 44.3 -> 24.8 (base-kok1)
and 24.1 (src-kok1), back to 43-44 within minutes. rgp_run.ps1's guard stops the run but cannot catch a single jump.
Needed to keep 31 GB through a run: about 51 GB free before it. Janitor has 0 candidates; the only content-int build is the
current one named by _pc/integration.json, so it is not deletable.

## Box hunt state at session end, 2026-11-08T18:47Z (read this first; supersedes the "Being tested" line above)
Tested on frames 9000-11000 (Saria scene) with shots, source build, override dirs via NX2NAVI_SPV_IID:
- Skip composite 3bdeddefe0d8de4b: box gone, all fog gone. It is the isolating pass, not necessarily the cause.
- Self-read fix (NX2NAVI_FB_LEARN_ALL): box stays. Mask bypass (ov3): box stays. c4[96]=1,1,1,0: box stays.
- ov4 "flagoff" (%426 = sky only): box gone in all frames, 60 fps, but haze reduced. NOT a fix. It changed two things at once:
  F gates the H1 blend (T = F ? H1.w : 1, add = F ? H1.rgb : 0) and also gates the fog block (G = notsky && (F || m <= 0)).
- ov5 (0.2% tolerance on zlin <= -zhw): box stays, so a small depth mismatch is not the cause (tol1_edges.png).
READY, NOT RUN (stopped on purpose at session end, no results exist): ov6 = no H1 term, gate unchanged; ov7 = fog block off, H1 unchanged.
Reasoning so far (inference, unproven): H1 = slot 8 = tex 23454, H2 = slot 9 = tex 24871, both rgba16f 960x540, written by pass 177
(fs bc280fb2a7cc7e22) and pass 179 (fs 1aba5cda2b65417f, 4 draws). Each was skipped ALONE earlier and the box stayed, which is what you would see
if BOTH carry it; skipping both together has never been run. Inputs they share: 3D volumes 884 and 882 (r11g11b10f 128x72x96) and LUT 746.
1920/128 = 15 px, the box's step size. But skipping injection (dbe3ccba + 67ebcf9b) kept the box, so an empty or stale volume does not clear it.
Dump PNGs drop alpha, and T = H1.w is alpha: check alpha separately before calling a buffer clean.
Also seen, unexplained: a black leaf-shaped hole upper left in tol1_full.png, smaller dark leaf shapes in other frames (falling leaves rendered black,
or caused by the edit? check a control); maxf 69-76 ms in 9000-10900 in every recent run, with shots on (shot cost not separated). Soak2 without shots: worst window 23.96.
Async replay A/B: 16.67 and 18.23 vs 17.79 and 17.38, 0 spike runs: no clear gain, stays off.
Newest Hookshot source to check, not read yet: Desktop\NEWEST LICHI EMULATOR BUILD\Hookshot-2.0.2.

## Session 2 box batch, expected results written BEFORE the runs (2026-11-08T18:59Z)
Saria scene, shots every 8 s from 150 s, source build eec5854. Control = no edit, same shots.
- box-ctl (control): box in most of the shots (it comes and goes). Decides the leaf hole: holes here too = real bug, not the ov5 edit.
- box-ov6 (no H1 term, gate unchanged). Expected: box GONE, fog block intact. Reason: ov4 removed the box and F gates H1, and pass 179's haze buffer shows the box.
  If the box stays: H1 is not the carrier and the fog block is.
- box-ov7 (fog block off, H1 unchanged). Expected: box STAYS, pale fog reduced. If the box goes instead: the carrier is the fog block (or G).
- box-skip2 (skip fs bc280fb2a7cc7e22 + 1aba5cda2b65417f together). Expected: box GONE and haze buffers constant, which would mean the box comes through those two passes. If it stays: it is not in those buffers at all.

## Naming and permission (owner, 2026-11-08T19:07Z)
Our build and the repo are named "Ocarina of Lain". Icon: Desktop\NEWEST LICHI EMULATOR BUILD\Ocarina Of Lain.ico. chrg6's words are saved in nx2navi-tools\PERMISSIONS.txt: full permission to provide the code under our own GitHub repo, must be named something else, reference the work taken from chrg6, do not advertise it as NX2Native. So step F also needs: exe renamed, icon in the exe, README credit. nx2navi-src\PERMISSION.txt still has only the first line.

## Disk guard stopped the first batch (2026-11-08T19:04Z)
box-ctl, box-ov6, box-ov7 all stopped at frame 3180-3240 (Kokiri load): the page file jump took C: from 42 to under the 22 GB guard. No results. Cleared shots and dumps from my old run folders (logs kept): 1.1 GB -> 13 MB. Need about 43 GB free before a run.
Hookshot 2.0.2 read: hookshot-2.0.2-leads.md. Top lead: read-only storage buffers bound past their size word (fog light probes read 0 past the binding, overbright haze). Item 2 there (float bits on the fog CS) is already covered by the mask 9 test (all CS, box stayed).

## Box: H1 is the carrier (Confirmed, 2026-11-08T19:24Z)
Runs box-ctl, box-ov6, box-ov7 (shots every 8 s from 150 s; box-ov6_edges.png etc). All 11040 frames, no fault, disk low 31.4 GB. Interval 17.46 / 17.61 / 17.43, GPU 7.56 / 7.64 / 7.57.
- ctl: box visible in the left strip in the 3 game shots (shots 4 and 5 are the Settings screen, cause unknown: only this run).
- ov6 (no H1 term): box GONE in all 5 shots. As predicted. Also removes the pale haze, so not a fix.
- ov7 (fog block off, H1 kept): box STAYS and is clearer, hard vertical edge in all 5. As predicted. So the fog block is cleared.
So the box is in what passes 177 and 179 write to t1 = tex 23454. Those passes draw 1 and 4 geometry draws (not full-screen compute), read BC4/BC5 256x256 textures, the froxel volumes, LUT 746, and the storage buffers at c0+0x510/0x520.
Shark's Hookshot tree: shark-amd-leads.md. An older Hookshot base; its NaN guard and Invariant are copies of our patches. Left to test: Invariant on mesh Position (MeshSpirv.cs has none), submit_finish 1 for the flash.
Next, box-stretch (NX2NAVI_SSBO_STRETCH=1, read-only storage buffers reach the end of their backing). Expected if the Hookshot lead is right: box gone, haze kept. If the box stays, reads past the size word are not the cause; then box-skip2.

## Box: two more results (2026-11-08T19:35Z)
- box-stretch (read-only SSBOs to the end of backing): box STAYS (box-stretch_edges.png). Hookshot's probe lead rejected for the box. Interval 18.23, replay 10.45 vs 9.06, one run only.
- box-skip2 (fs bc280fb2 + 1aba5cda both skipped): box STAYS and is sharper, in both strips (box-skip2_edges.png). My prediction (gone) was WRONG. So the box does not come from what those two passes draw.
Note ov6 only removes the box together with the F branch: T = F ? H1.w : 1, add = F ? H1.rgb : 0. With H1 undrawn the difference left is F itself, so F's inputs are next: r32f tex 26809 and d32f tex 26846.
Reading the strip: the OUTER band is the grey washed part, the inside looks saturated.
Black leaves show in skip2 and ov6 too (a black leaf in shot 1 of skip2), so they are not the ov5 edit.

## Box: what the dump shows (2026-11-08T19:41Z) (runs/dump-f, dumpf_a.png)
d32f tex 26846 (pass 168 td) is 1.0 nearly everywhere (18 distinct values, 0.995 in one corner). So F is true nearly everywhere and ov4 was in effect the same edit as ov6. F is cleared: the depth inputs carry no box.
H1 rgb (pass 179 t1) carries the box: inside warm, outer band blue, hard stepped edge. H1 alpha shows the ground shape with no such split at this view. Composite pass 197 t0 has the box in the same place.
Open contradiction: box-skip2 still shows a box though H1 is undrawn. Dump it to see what H1 and the scene hold there.
Next: box-skipd = skip2 with a dump of passes 179,195,197. Expected: H1 flat; the box already in pass 195 if it comes from the lit passes.
box-ov8 (fs 1aba, iid cca3fa94f27045af): cluster light count forced to 0, only the sun term runs. Expected: if the warm light is a clustered light, the warm inside goes and the box goes. If the box stays warm: it is the sun term or the volumes.

## Box: H1 is drawn by particles (2026-11-08T20:00Z)
box-ov8 (cluster lights off, sun only): box STAYS (box-ov8_edges.png), so the cluster list is cleared.
dump-v: fog volumes 882/884/886 show no x band near the box's edge (dumpv.png; 8-bit view, coarse). Volumes cleared again.
Decompiled shaders: shaders/h_1aba.glsl, h_cfe7.glsl. The 4 draws of fs 1aba are instanced particles: the VS pulls each instance from storage buffers (start time, life) and takes the sun visibility per vertex from volume 886. H1 rgb = particle colour, warm where the sun term counts. Alpha = vertex fade * BC5 texture (tex 665, swizzle RRRG, correct) * soft depth fade. Edge finder: strongest straight column in H1 at x = 910-911 of 960 at frame 9600 (5.1% from the right).
New idea: the box is the outline of one large near-camera particle whose edge should fade out. Still open: box-skip2 shows a box without these draws.
Next, expected first:
- box-skipd2: skip2 with H1 (pass 186) and composite (pass 204) dumped, plus tex 665. Expected: H1 flat. If H1 is not flat, something else writes it.
- box-skipv1 (skip vs cfe721e49a065f76, 3 draws) and box-skipv2 (skip vs 8946e1d76cc23b6b, 1 draw). Expected: the box goes with exactly one of them if it is one particle system. If it stays in both, the outline idea is wrong.

## Box: outline idea rejected (2026-11-08T20:21Z)
box-skipv1 (skip vs cfe721e, 3 draws) and box-skipv2 (skip vs 8946e1d, 1 draw): box STAYS in both. Not one card's outline. These draws include falling leaves (rotating meshes with a leaf texture).
box-skipd2 missed H1: pass numbers move with skips. In the skip run H1's passes are 183-185, in the normal run 177-179.
Next: box-h1n = normal run dumping 177,178 (H1 before the leaf draws); box-h1s = skip2 run dumping 183,184,185. Expected: box-h1s H1 flat; if not flat, something else writes H1 or it holds stale data, which would explain why the skip does not clear the box.

## Box: H1 is written by a MESH draw that the dump log hides (Confirmed, 2026-11-08T20:31Z)
box-h1n: H1 is flat (0,0,0,1) after pass 177; after pass 178 it has the full ground haze (458,441 distinct values) though the log says "draws 0".
box-h1s (bc280 + 1aba skipped): flat after pass 182; after pass 183 ("draws 0") full content, 465,333 distinct values.
vk_exec_draw.c:3048 reads: if (vk_dump_armed && !mesh) vk_dump_draw(...). Mesh draws never reach the log, so every "draws 0" pass may hold them.
This explains box-skip2: the skip never touched H1's real writer. The earlier readings of 1aba/bc280 as H1's writers were wrong; 1aba only adds on top in the next pass.

## Box: the mesh draw that writes H1 (2026-11-08T20:40Z)
Pass 178: mesh 1364e8dc52ae9172 + fs 560e3938562c8966 (iid f55edb08d45ce981; shaders/m_560e.glsl). It marches a ray through a 512x512 rgba16f map (tex 874: R = ground height, stepped edges; G = thickness, only in splotches), at most 15 units. If the march hits nothing (%157 = 0) it takes a separate path. Its other depth map (480x270 rg32f, pass 114) is empty on this frame: rejected.
box-ov9: debug output in H1: red = march hit, green = no hit, blue = march length / 15, alpha 1. Expected: if the warm/blue split is the hit test, the box edge shows as the red/green boundary. If the boundary is elsewhere, the split comes from later in the shader.
Uniforms of fs 560e at frame 9600 (runs/box-ubo/p560.txt): map 32 world units wide at origin (-37, -4), camera at (-27.94, 114.43, 6.41), ground about y 110.6. So the map edge is far outside this view: the map edge is not the box. c5[40].w = 0, so the 480x270 rg32f branch never runs on this frame.

## Box: split the fog shader's colour terms (2026-11-08T20:47Z)
box-ov9 (dump_ov9.png): the march hits on 89% of pixels, no box in the hit mask. So the march is cleared; the split comes from a colour term.
Final colour in fs 560e: rgb = (ambient _295 + lights _297 + sun _303 + constant _309) * c5[36] * exp(-fog) + fog. Debug runs paint terms into H1:
ova: sun _1161, ambient scale _1159, light scale _1157. ovb: ambient _295/_287/_291. ovc: lights _297/_289/_293.
Expected: the box edge shows in exactly one. My guess is the sun term (_1161), because the inside of the box is warm and it comes from volume 886.

## Box is in the AMBIENT term of fs 560e (Confirmed, 2026-11-08T20:58Z; dump_abc.png)
ova (sun, scales): no box. ovc (lights): small glow, no box. ovb (ambient _295/_287/_291 = _887/_885/_889 * scale): BOX, yellow inside, grey-white outside, hard stepped edge, a trapezoid in perspective, top edge visible too. My sun guess was wrong. So a world-space ambient region, probably a probe volume.

## Ambient comes from a sparse probe grid read through pointers (2026-11-08T20:59Z)
fs 560e lines 1216-1264, 1613-1642: s1 (0x40-byte buffer at 0x1af9b6540) holds 5 pointers. s2 header: origin +16/+20/+24, cell size +28, dims +32/+36/+40. s3 strides. s4 brick table, s5 brick indirection, s6 spherical-harmonic data (0.282 = L0). Cell index is clamped to the grid; each read is (address - pointer) >> 2, low 32 bits.
Hypothesis to test: bricks past a binding's end read 0 under robust access. That would give a stepped world-space edge. Hookshot's comment describes this for its own build. Running box-ptr: logs addr and bound size for each pointer resource.
Expected: at least one of s4/s5/s6 bound shorter than the data it needs. If all reach far enough, the zero-read idea is out and the probe contents are next.

## Probe grid data (2026-11-08T21:06Z) (runs/box-grid, runs/box-ptr)
All pointers bound to the pool end (about 150 MB, s6 121 MB): the binding-size idea is rejected at the level of the bind.
Header: origin (-208, 98, -126), cell 64, dims 6 x 2 x 4. Brick table entries {data offset, resolution 8/16/32, ?, ?}. Camera (-28, 114, 6.4), so cell boundaries at x = -16 and z = 2 fall near the view.
box-ovd: paints fract of the cell coordinates. Expected: if the box is a 64-unit cell (brick) edge, a colour jump lines up with its edge. If not, the box is inside one brick and comes from the voxel data.

## Probe grid was the wrong branch (2026-11-08T21:11Z)
box-ovd painted all zeros: the probe branch does not run on this view. Ambient is instead tex 889 (64x36x96 r11g11b10f, written by pass 76, cs 67ebcf9b, as img1) sampled at the march's centroid: u, v from projecting it, slice from log view depth (m_560e.glsl lines 959-1039). I was wrong that the grid was the source.
dump_889.png: slices show no vertical line at the box's x. Slices are dim and blue up to about 20, bright yellow from 22, black behind the ground (unwritten cells, stepped by group). So a small slice change means a big colour change.
box-ove: paints u, v, depth/8. box-ovf: paints fract of the centroid. Expected: a kink at the box edge in the depth or centroid would mean the coordinate jumps; no kink means the volume data is sampled smoothly and the volume itself needs another look at full precision.

## Friend's lead: Shakoquo/Hookshot release amd-fix-2.0 (2026-11-08T21:13Z)
Owner's friend says this build fixed the box. Its notes do not name the box. They list: wait for the GPU each frame (issue 574), Invariant positions on AMD, "plant shaders read constants as a buffer address", "water and light-probe fixes". Hookshot 2.0.4 is also on the Desktop (diff in shark-vs-hs202/hs204.diff, 849 KB, not read yet).
Next tests, one variable each, expected first:
- box-sf1: NX2NAVI_SUBMIT_FINISH=1. Expected if the CPU overwrites data a later submission still reads: box gone. Costs fps. Do not ship until flash3 and timing are checked.
- box-inv: Invariant on mesh Position (load-time switch NX2NAVI_MESH_INVARIANT=1). Expected: helps leaf holes and depth-equal gaps; box unlikely, since the hit mask is clean, but the friend reports a fix so it is tested.

## CORRECTION: the float-bits mask never worked (2026-11-08T21:14Z)
spirv_float_bits read NX2NAVI_FLOAT_BITS through rt_knob, which returns only 0, 1 or 2. So the mask runs (3, 5, 9) all ran as mesh and task only. The rejection "float bits on VS, FS or CS: box stays" is VOID. "None brings the noise back" was the same bug. What is known: all stages (older code) brought noise; mesh and task (default) do not. Fixed: the value is read as a number. Next: real masks, box-fb5 (mesh + fragment), box-fb9 (mesh + compute), box-fb3 (mesh + vertex). Expected: fs 560e reads tex 889 and uses integers held in float variables, so one of these may clear the box; one of them is also the source of the noise.

## Box: out-of-bounds texelFetch hypothesis (2026-11-08T21:18Z)
dump_coords.png: u, v, depth and the centroid are all smooth, no kink at the box. But box-ove's ranges: u -0.073 to 1.065, v -0.085 to 0.928. The march centroid projects off-screen in a band at left, right and top, and not at the bottom. That is the trapezoid's three edges.
m_560e.glsl line 1019: texelFetch(tcb_C, ivec2(_341, _349), 0) with those coordinates, then _360 = (_323 >= _358) picks volume 889 or the other path. Out-of-bounds texelFetch is undefined in Vulkan; on this card it may return 0, giving depth -0.1 and _360 false.
box-ovh: clamp that one fetch to 0..959, 0..539 (fs iid f55edb08d45ce981). Expected: box GONE in all shots, haze kept, ambient smooth. If the box stays, this hypothesis is wrong.
Float mask batch stopped before it finished, to test this first; redo it later.

## BOX ROOT CAUSE FOUND (Confirmed on one shader, 2026-11-08T21:23Z)
box-ovh (fs 560e with its one out-of-range texelFetch clamped): box GONE in all 5 shots, haze kept (box-ovh_edges.png). Owner saw it in the live window too. Edge finder: only the window's side bars remain. Interval 18.49, GPU 7.43 (one run, dump on).
Cause: the fog shader projects its march centroid to the screen and reads depth with texelFetch; in a band at left, right and top the point is off-screen (u -0.07..1.07, v -0.085..). Out-of-bounds fetch is undefined in Vulkan. On the RX 9070 it sends those pixels to the other ambient path: grey-white instead of volume 889's value. 15 px steps are 2 x 8 px texels of the low-res buffers.
Note: black leaf at the top of the owner's shot is a separate open bug.
Next: root fix at shader load (spirv_fetch_clamp), all stages, validated with spirv-val over the shader cache; then flash3, A/B, soak, install.

Root fix built: gfx_shader.c spirv_fetch_clamp, all stages, on by default, NX2NAVI_FETCH_CLAMP=0 turns it off. Checks to run: fetchclamp_val.py over the whole cache; box-root (no override) expected box gone in all shots, haze kept; control is box-ctl. Then flash3, A/B, soak.

## Root fix proven on the Saria scene (2026-11-08T21:29Z)
box-root (src commit with spirv_fetch_clamp, no override): box GONE in all 5 shots, haze kept (box-root_edges.png), 0 faults, interval 17.44, GPU 9.14 (one run, shots on). Whole cache: 36,727 modules, 338 changed, 335 valid, 0 made invalid by the clamp, 3 invalid before it too.
Next: alternating A/B against NX2NAVI_FETCH_CLAMP=0 in Kokiri and Deku, flash3, install, soak, true fullscreen.
A/B started 2026-11-08T21:29Z: ab-kok (3420-3720) and ab-deku (11000-12000), NX2NAVI_FETCH_CLAMP 0 vs 1, 3 reps, alternating. Expected: no measurable cost; only 338 modules gain a few instructions.

## Clamp A/B, Kokiri heavy view (2026-11-08T21:38Z, runs/ab-kok.txt; 3 alternating pairs)
off: interval 17.77, replay 13.45 (13.07-13.71), GPU 8.17, worst 21.07. on: interval 18.21, replay 13.67 (13.58-13.69), GPU 8.32, worst 25.54. Each side has one run at GPU 11 (off-3, on-2), so this is noise-level: no clear cost, small cost not ruled out. Finish line B not met in Kokiri on either: about 18 ms, CPU replay 13.5.
Next: flash runs. flash-k: FLIGHT_FRAMES 2600 at 3700, 6300, 8900 (Kokiri load and walk). flash-d: togame route, FLIGHT_FRAMES 1600 at 12400 (Deku). Expected 0 spike runs in each. Then ab-deku.

## Flash check, first pass (2026-11-08T21:47Z)
flash-k: dumps cut off by the run's end. 5,519 frames 1131-6649: 8 spike runs around 1%: 1225-1234, 1239-1242, 1256-1275, 1291-1306, 1376-1377, 1636-1658, 5944, 5946-5947. flash-d: 921 frames 10831-11751: 0 runs. flash3.py now reports empty and unreadable files instead of stopping (a file it cannot read is counted, never passed silently).
Next: flash-k0 (clamp off) and flash-k1 (on), longer so dumps finish, flash-d1. Expected: the same runs with the clamp off, then they are not from this change.

## INSTALLED 2026-11-08T21:50Z (owner's go-ahead)
game\ocarina.exe = src 4eb28d7 (d8ba170's fixes + fetch clamp), SHA256 AAB63318...9AF7, matches the build. Backup of the previous exe: game\ocarina.exe.bak-d8ba170. Ini untouched, submit_finish = 0x1. Rollback: copy that backup over ocarina.exe.
Installed before the flash control finished, on the owner's word. Still to check on the installed copy: flash-k0/k1/d1, true fullscreen, 10-minute soak, Deku A/B. Owner's own fullscreen play is finish line E.

## STATE at 2026-11-08T22:16Z (read this first)
INSTALLED: game\ocarina.exe = src 654f63c runtime (fetch clamp, name Ocarina of Lain, icon, settings line "Ocarina of Lain 1.0"). SHA256 C32479D4...B88E. Backups: ocarina.exe.bak-4eb28d7, .bak-d8ba170, .bak-0.5.0-original. Ini untouched, submit_finish 0x1.
REPO: github.com/jabroni1kanobi/Ocarina-of-Lain, PRIVATE (owner's choice), branch main = 654f63c, remote name lain. check_content passes. Push only with the owner's say.
Flash (flash-batch2): clamp off 7 runs, clamp on 7 runs, same places (title screen grass 1229-1669; camera swing near 6051-6115), 1% of pixels, 0 faults: not from the clamp. Deku 10831-12430: 0 runs in 1,600 frames.
Smoke of the final exe: Kokiri load to frame 3720, 0 faults. The settings line itself was not seen on screen by me: owner to confirm. Window title confirmed by the process.
OPEN, in order:
1. Ghosting on leaves and parts of Link (owner sighting, 2026-11-08), black leaves. Ideas: NX2NAVI_MESH_INVARIANT=1 (built, untested); motion vectors or FSR on those draws; falling leaves draw through fs 1aba + vs cfe721e/8946e1d into H1 at half resolution, composited in pass 197.
2. Owner's fullscreen confirmation through the launcher (finish line E) and black side bars in true fullscreen: not checked by me.
3. 10-minute soak on this build, voices (finish line D): not run on it.
4. Deku A/B not re-run since the clamp. Kokiri about 18 ms, CPU replay 13.5 ms: line B not met. Leads: runtime at AVX2, compute write extent (upstream-leads.md #1).
5. Redo float mask runs (box-fb5, fb9, fb3): the mask was broken before 4eb28d7, so VS, FS and CS were never tested alone. Hookshot 2.0.4 diff not read (shark-vs-hs202/hs204.diff).

## Rename 2026-11-08T22:19Z (owner's order: remove the old name everywhere, builder too)
Repo: display text = Ocarina of Lain, file and folder names = OcarinaOfLain (no spaces: build tools live under these paths), updater feed = this repo. chrg6's original README removed from the tree (old history keeps it). LICENSE and LICENSE-ADDITIONS untouched. Kept, because compiled game objects and the owner's data depend on them or they need a full rebuild from the kit: lowercase nx2navi file names (nx2navi.ini, nx2navi.log, nx2navi.rc), NX2NAVI_ settings prefix, C symbols. Git history keeps old text.
Owner's installed launcher (app/0.5.0/launcher): display text only + feed; folder and OcarinaOfLain.exe keep their names, shortcuts and tools depend on them. Original: app/0.5.0/launcher.bak-original.
Banner: assets/ocarina-of-lain.jpeg at the top of the README.
