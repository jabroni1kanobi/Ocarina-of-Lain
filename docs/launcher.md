# Launcher

NX2navi's launcher installs everything the build needs, builds the game from the player's own dump, starts it, and
keeps the port up to date. Players download one file, `NX2navi-Setup-<version>.exe`; nothing of the game ships with
it. The code is `launcher/`, the release is made by `tools/make_launcher.py`, and the build itself is
`tools/make_game.py` (the build kit's script, [README-BuildKit.md](README-BuildKit.md)).

## What the player does

1. Starts `NX2navi-Setup.exe` (from the folder that holds their `.nup` dump, ideally). It installs the launcher into
   `%LOCALAPPDATA%\Programs\NX2navi` without administrator rights, adds Start menu and Desktop shortcuts and an entry
   in Settings > Apps, and opens the launcher. Windows SmartScreen warns once: the programs are not signed.
2. The launcher finds the dump by itself in the setup's folder, Downloads, Desktop, Documents or the top of each
   drive, and proposes `<launcher>\game` as the install folder (or `X:\Games\NX2navi` on the drive with the most
   free space when that drive has less than 40 GB). Both rows can be changed.
3. Selects **Begin Installation**. The launcher downloads the build tools (about 170 MB, once), checks the dump,
   unpacks it, translates and compiles the game: 10 to 25 minutes. It keeps the PC awake meanwhile.
4. Selects **Play**. The launcher hides while the game runs and comes back when it exits.

From then on the launcher looks for a new version each time it opens (Settings can turn that off). When one is out,
the Updates row turns orange; selecting it downloads the new version, restarts the launcher in it and rebuilds the
game. Saves and settings are never touched.

## Look

The window follows the game's options menu, with colours sampled from it: a wooden title band with the Triforce and
the name, bronze label cells, dark value cells, orange On/Off choices, a white glowing cursor around the selected
row, the menu's filigree dividers and a description box for the selected row. All text is in the game's own Cetra
Display: the launcher decodes it from the player's dump or built game into `data/cache/fonts` (as
`runtime/menu_assets.c` does for the in-game menu) and uses the system's interface font until then. Every picture is
drawn in code (`launcher/ui/art.py`); no texture, font or sound of the game is in the repository or the release.

The Triforce is also the icon of `NX2navi.exe`, `NX2navi-Setup.exe`, the window and the taskbar
(`art.triforce_ico`, written into the release by `tools/make_launcher.py`).

Screens (`launcher/ui/app.py`): first run (dump, install folder, Desktop Shortcut, Check for Updates at Start,
Update Channel, Begin Installation); the steps of an install, update or rebuild with their progress, the build log's
last lines and Cancel, and after a failure Try Again and Copy Report; home (Play, Updates, Game Folder, Rebuild the
Game, Settings); settings (update check, Hide Launcher While Playing, Keep Build Cache, Update Channel, the folders,
Open Logs Folder, Uninstall, Back); uninstall (keep or remove the built game).

Controls: the mouse (hover moves the cursor, a click selects); the keyboard (arrows, Enter or Space, Esc, F5 checks
for updates); an XInput gamepad (d-pad or left stick, A, B, Y).

`python -m launcher --selftest --shots DIR` draws every screen with made-up states and saves a PNG of each.

## The launcher's folder

```
NX2navi.exe              launcher/native/nx2navi.c: swaps in a staged Python, starts app\<current>\launcher\start.pyw
python/                  python.org's embeddable Python 3.14.9 with tkinter, numpy, lz4 and zstandard
app/current.txt          the version the stub starts; app/previous.txt the one before it
app/<version>/           launcher/, runtime/, recomp/, tools/, bin/nx2navi_xlate.exe, VERSION,
                         toolchain.lock.json (the pinned build tools), feed.json (where updates come from)
pydeps/                  zbic, downloaded from PyPI on this PC
toolchain/msys64/ucrt64  MSYS2's clang and libraries, downloaded from the MSYS2 repository on this PC
toolchain/pkgs/          the downloaded package files
data/launcher.json       settings; data/logs/launcher.log (and crash.log); data/cache/fonts
game/                    the built game, unless another folder was chosen (its layout: README-BuildKit.md)
```

A path the Windows ANSI code page cannot hold breaks the MSYS2 tools, so the toolchain then goes to
`C:\NX2navi\toolchain` instead (`launcher/paths.py`).

## Installing

`jobs.InstallJob`, one row per step:

1. **Gather Build Tools** (`launcher/toolchain.py`): the packages `toolchain.lock.json` pins (MSYS2 UCRT64 clang,
   gcc, SDL3, Opus, libvpx, dav1d, zstd, Vulkan headers and loader, VMA and their dependencies: 31 packages, 170 MB),
   each checked against its sha256 and unpacked into `toolchain/msys64.new`, which replaces `toolchain/msys64` once
   complete. No MSYS2 shell or pacman runs. Then zbic: the wheel for the bundled Python from PyPI, checked against
   PyPI's sha256, into `pydeps/`.
2. **Inspect Your Game** to **Compile**: `tools/make_game.py --nup <dump> --out <game> --msys <toolchain> --events`,
   whose `@@` progress lines drive the rows (check, exefs, romfs, lift, compile).
3. **Prepare the Shaders**: make_game.py's last step translates every shader of the game into the game's shader
   cache and the driver's own cache (`tools/prepare_shaders.py`), so play does not stop to compile them.
4. The game's Cetra Display is decoded for the window.

## Updating

A release carries `NX2navi-update.json` (version, notes, and the size and sha256 of each file). The feed
(`app/<version>/feed.json`) names a GitHub repository: the launcher reads its newest release (or pre-release, on the
Preview channel) that has the manifest. A private repository answers only with a token (`github_token` in
`data/launcher.json`); players need a public one. `feed_repo` in `data/launcher.json` points one launcher at another
repository for testing; `{"type": "url", "url": ".../NX2navi-update.json"}` in feed.json reads one manifest
directly.

`jobs.UpdateDownloadJob`, before the restart (`launcher/updater.py`):

1. **Download** `NX2navi-app-<version>.zip` (about 3 MB plus the shader translator) to `data/downloads`, resuming a cut
   download, and check its size and sha256. When the manifest names a Python other than `python/runtime-id.txt`, also
   `NX2navi-runtime-<id>.zip`, unpacked into `python.next`.
2. **Verify**: the archive unpacks into `app/<version>.part`, refusing names that leave the folder, holds the expected
   `VERSION` and a launcher, and becomes `app/<version>`.
3. `app/previous.txt` gets the running version, `app/current.txt` the new one, and the launcher starts the stub with
   `--continue-update <old>` and closes.

The stub swaps `python.next` in while no Python runs, then starts the new version, which finishes in
`jobs.ContinueUpdateJob`:

4. **Update Build Tools**: only when the new release pins other packages.
5. **Translate the Code** and **Compile Changed Files**: `make_game.py --rebuild`. The exefs and romfs steps depend
   only on the game files, so they are kept and the dump is not needed again; the lift runs again and the compile
   step compiles only the files whose source, included headers, flags or compiler changed (`build-cache/obj/
   objects.json`, from clang's dependency files).
6. **Swap In the New Build**: the link writes `ocarina.exe.new`, which replaces `ocarina.exe` only once it is
   complete. A failed rebuild leaves the old build playable; Rebuild the Game tries again.
7. **Prepare the Shaders** again for the new build; shaders the translator already prepared come from the cache.
8. Every version but the current and the previous one is deleted.

When the new version fails to start (exit code 86 from `start.pyw`, or any error within six seconds), the stub sets
`current.txt` back to `previous.txt`'s version and starts it with `--rolled-back`. The game is never rebuilt while it
runs. Keep Build Cache off deletes the lifted C and the objects (about 5 GB) after each build but keeps
`build-cache/state.json`, so updates still rebuild without the dump (compiling everything).

## Uninstalling

Settings > Uninstall NX2navi, or Settings > Apps > NX2navi (which runs `NX2navi.exe --uninstall`): the launcher
folder, its toolchain, the shortcuts and the Apps entry go; the built game and its saves stay unless "Remove the
Built Game" is On. The dump is never touched.

## Command line

`launcher/cli.py` runs the same jobs without the window (`python -m launcher.cli`, or in an installed launcher
`python\python.exe app\<version>\launcher\cli.py`): `check`, `update`, `tools`, `install --dump F --game D`,
`rebuild [--game D]`, `fonts`. `NX2navi.exe --desktop-shortcut on|off` and `--shortcuts on|off` are the stub's own.
`NX2navi-Setup.exe` takes `--dest DIR`, `--no-shortcuts`, `--no-launch` and `--quiet`.

## Releases

1. Bump `VERSION`; build the shader translator (`tools/build_xlate.py`).
2. With Python 3.14, numpy (the icon is drawn with it, `launcher/ui/art.py`) and uv:

   ```powershell
   uv run --no-project --python 3.14 --with numpy tools\make_launcher.py --xlate build\xlate --ffx amd_fidelityfx_vk.dll --feed owner/name --notes notes.md
   ```

   `--ffx` puts AMD's signed `amd_fidelityfx_vk.dll` (FidelityFX SDK 1.1.4) in the release for FSR 3.1.

   On Linux it cross-compiles the two programs with clang, lld and llvm-windres against the same MSYS2 packages and
   unpacks Tcl/Tk with msiextract (msitools). It writes into `build/launcher-release`: `NX2navi-Setup-<version>.exe`
   (the setup program with the launcher folder appended as a zip), `NX2navi-app-<version>.zip`,
   `NX2navi-runtime-<id>.zip` and `NX2navi-update.json`. `check_content` runs over the folder first.
3. Upload those four files to GitHub release `v<version>` of the feed repository, by hand or with `--publish`
   (`gh release create`; add `--draft` to look before it goes out). Launchers see it at their next start.

The release resolves the MSYS2 packages of the day into `toolchain.lock.json` (`--lock` reuses an older lock). The
repository keeps old package files, so a release keeps building with the compiler it was tested with.

## Tests

`python tests/launcher_test.py` (Python 3.14): versions, settings, the toolchain resolver and installer against a
local fake repository, resumed and checked downloads, the GitHub and URL feeds, staging, switching, rollback and
pruning, unsafe archives, the font decoding on a system font wrapped like the game's, `make_game.py` on a synthetic
`.nup` (events, skipped steps, `--rebuild` without the dump, step keys), the compile step's object cache with the
system's C compiler, the install job with a fake `make_game.py`, and the window's selftest when a display is there.

## Requirements and limits

- Windows 10 or 11, x86-64; the build's own requirements (README-BuildKit.md): 16 GB of RAM, about 30 GB free for
  the game and 1.5 GB for the launcher and its tools, a Vulkan 1.3 GPU.
- The programs are not signed: SmartScreen warns on the first start of the setup.
- English only for now.
- The launcher builds the Windows game only; Linux builds use `docs/building.md`.
