"""Assemble the build kit: what a player needs to build Ocarina of Time on their own PC with tools/make_game.py.

    python tools/make_kit.py --xlate <dir with nx2navi_xlate.exe and .id> [--out build/kit] [--zip]

The kit holds this repository's runtime/, recomp/, the build tools, the shader translator built from tools/xlate and
README-BuildKit.md; no game code, data or anything generated from the game. check_content runs over the folder before
it is zipped.
"""
import argparse
import shutil
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NAME = 'NX2navi-BuildKit'
TOOLS = ('make_game.py', 'nup.py', 'romfs_index.py', 'check_content.py', 'hostmem.py', 'prepare_shaders.py',
         'shader_census.py', 'shader_list.py', 'zstd_compat.py')
TOP = ('LICENSE', 'LICENSE-ADDITIONS', 'THIRD_PARTY_NOTICES.md', 'VERSION', 'nx2navi.ini.example')
SKIP_DIRS = {'__pycache__', '.pytest_cache'}
SKIP_SUFFIXES = {'.o', '.obj', '.pyc', '.exe', '.dll', '.a', '.lib'}


def copy_tree(src, dst):
    for p in sorted(src.rglob('*')):
        rel = p.relative_to(src)
        if any(part in SKIP_DIRS for part in rel.parts) or p.is_dir() or p.suffix.lower() in SKIP_SUFFIXES:
            continue
        (dst / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(p, dst / rel)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--xlate', type=Path, required=True, help='folder with nx2navi_xlate.exe and nx2navi_xlate.id')
    ap.add_argument('--ffx', type=Path, help="AMD's signed amd_fidelityfx_vk.dll (FidelityFX SDK 1.1.4, MIT) for FSR 3.1")
    ap.add_argument('--out', type=Path, default=ROOT / 'build' / 'kit')
    ap.add_argument('--zip', action='store_true')
    a = ap.parse_args()
    kit = a.out / NAME
    if kit.exists():
        shutil.rmtree(kit)
    kit.mkdir(parents=True)
    for d in ('runtime', 'recomp'):
        copy_tree(ROOT / d, kit / d)
    (kit / 'tools').mkdir()
    for t in TOOLS:
        shutil.copy2(ROOT / 'tools' / t, kit / 'tools' / t)
    for t in TOP:
        if (ROOT / t).is_file():
            shutil.copy2(ROOT / t, kit / t)
    shutil.copy2(ROOT / 'docs' / 'README-BuildKit.md', kit / 'README-BuildKit.md')
    (kit / 'bin').mkdir()
    for n in ('nx2navi_xlate.exe', 'nx2navi_xlate.id'):
        shutil.copy2(a.xlate / n, kit / 'bin' / n)
    if a.ffx:   # FSR 3.1 loads it from beside the game (runtime/vk_ffx.cpp); its licence is runtime/third_party/ffx_api
        shutil.copy2(a.ffx, kit / 'bin' / 'amd_fidelityfx_vk.dll')
    sys.path.insert(0, str(ROOT / 'tools'))
    import check_content
    bad = [f'{p.relative_to(kit)}: {m}' for p in sorted(kit.rglob('*')) if p.is_file()
           for m in check_content.problems_for(p, allow_large=p.suffix in ('.exe', '.dll'))]   # the translator, AMD's DLL
    if bad:
        sys.exit('check_content refuses these kit files, not zipped:\n  ' + '\n  '.join(bad[:20]))
    size = sum(p.stat().st_size for p in kit.rglob('*') if p.is_file())
    print(f'{kit}: {size / 1e6:.1f} MB')
    if a.zip:
        z = a.out / f'{NAME}.zip'
        with zipfile.ZipFile(z, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
            for p in sorted(kit.rglob('*')):
                if p.is_file():
                    zf.write(p, Path(NAME) / p.relative_to(kit))
        print(f'{z}: {z.stat().st_size / 1e6:.1f} MB')


if __name__ == '__main__':
    main()
