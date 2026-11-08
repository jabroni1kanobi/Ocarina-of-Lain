# Ocarina of Lain

A build of **NX2navi by chrg6**, the native PC recompilation of Ocarina of Time for Switch 2, with fixes for AMD graphics cards.

**All credit for the port goes to chrg6.** This repo is chrg6's source plus the changes listed below, and it is not the official NX2navi. Please take problems with this build here, not to chrg6. chrg6's original README is at [docs/NX2navi-README.md](docs/NX2navi-README.md), and chrg6's `LICENSE` and `LICENSE-ADDITIONS` files apply and are unchanged.

chrg6's permission for this repo: *"You have full permissions to provide the code under your own GitHub repo, but you must name it something else. Reference work taken from me, but do not advertise your build as NX2Native."*

No game files are in this repo. You need your own copy of the game.

## What we changed

| Fix | What you see | Where |
|---|---|---|
| Image fetches clamped to the image | The pale stepped box near the screen edges is gone, and the fog stays | `spirv_fetch_clamp` in `runtime/gfx_shader.c` |
| Float bits kept on mesh and task shaders | Leaf shadows no longer cover the whole shadow map: large GPU saving in wide outdoor views | `spirv_float_bits` in `runtime/gfx_shader.c` |
| Frame wait after the first submission | Flashes in Kokiri gone. Set `submit_finish = 0x1` in `nx2navi.ini` | your `nx2navi.ini` |
| AVX2 game code | Faster CPU side (optional, see step 4) | `tools/lain/build_guest.py` |
| New name and icon | Window title, settings menu, icon | `runtime/gfx_window_win.c`, `runtime/ui/menu.cpp`, `runtime/ocarina-of-lain.ico` |

The box's cause: the ground fog shader projects a point to the screen and reads depth there. Near the left, right and top edges that point is off screen, which Vulkan leaves undefined, and on an RX 9070 those pixels took a different lighting path. The full investigation is in [docs/lain-findings.md](docs/lain-findings.md).

## Results

Tested on one PC only: Radeon RX 9070, driver 26.10.1, at 2560x1440.

| Scene | Original 0.5.0 | Ocarina of Lain |
|---|---|---|
| Kokiri Forest, heavy wide view | frame about 39 ms, GPU 24 ms | frame about 18 ms, GPU about 8 ms |
| Deku Tree cutscene | GPU 31 ms | frame 16.7 ms, GPU 7.5 ms (measured before the box fix) |

The flash meter found no flashes in 1,600 frames of the Deku Tree scene. In Kokiri it found only small spikes, about 1% of the picture, and the same ones appear with the box fix turned off.

## Known problems

- Falling leaves, and parts of Link at times, show a ghosting effect. Some leaves draw black.
- Kokiri's heavy view is about 55 fps, not a locked 60. The limit there is CPU time.
- Only tested on the PC above. Other cards may behave differently.

## Build it

You need Windows, about 60 GB free, and your own game. The scripts relink the runtime against a working NX2navi install, so the install does the heavy work.

1. Install **NX2navi 0.5.0** with chrg6's launcher and let it build the game from your own files. Run it once.
2. Install Python 3.12 or newer. Clone this repo into a folder named `src` (for example `C:\Users\you\Games\nx2navi-src\src`).
3. Copy `tools/lain/build_rt.py` and `tools/lain/build_guest.py` up one level, next to `src`. Edit the paths at the top of both to match your PC: `ROOT`, `GAME`, `CACHE`, `MSYS`. They currently hold the paths from the PC this was built on.
4. Optional, faster on CPUs with AVX2: `python build_guest.py`. It writes `guest-x86-64-v3-O1`, and step 5 links it with `--guest`.
5. Build: `python build_rt.py --setup --guest guest-x86-64-v3-O1 --out exe-ms.exe`. Leave out `--guest ...` if you skipped step 4. The result is `out\exe-ms.exe`.
6. Install: in the NX2navi `game` folder, back up `ocarina.exe`, then copy `exe-ms.exe` over it, keeping the name `ocarina.exe` so the launcher still starts it.
7. In `nx2navi.ini`, under `[performance]`, set `submit_finish = 0x1`.

A launcher update overwrites the exe: repeat step 6 afterwards.

## Switches

Set as environment variables:

- `NX2NAVI_FETCH_CLAMP=0` turns the box fix off.
- `NX2NAVI_FLOAT_BITS` is a mask: 1 mesh and task (default), 2 vertex, 4 fragment, 8 compute. All stages at once gave a burst of noise in an older test.
- `NX2NAVI_MESH_INVARIANT=1` marks mesh positions invariant. Off by default, not tested yet.

## Test tools

`tools/lain/` holds the scripts used to test: `rgp_run.ps1` (one run), `ab.ps1` (alternating timing runs), `metrics.py`, `flash3.py` (flash meter), and the shader debug helpers. They hold paths from the build PC, so edit them first. They contain no game data.
