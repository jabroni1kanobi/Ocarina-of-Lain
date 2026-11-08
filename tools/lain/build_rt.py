"""Compile runtime/ of this source copy and relink ocarina.exe against the game objects already built in the install.

Nothing is written into the install: game objects (main_*, sdk_*, subsdk0_*, modules, shim, rc) are read from its
build-cache; runtime objects and the exe go under nx2navi-src. Flags and libs match tools/make_game.py step_compile.
    python build_rt.py [--jobs 8] [--setup]
"""
import argparse, concurrent.futures, os, re, shutil, subprocess, sys, time
from pathlib import Path

ROOT = Path(r'C:\Users\Johnn\Games\nx2navi-src')
SRC = ROOT / 'src'
RT = SRC / 'runtime'
OBJ = ROOT / 'obj'
RT_MARCH = []
OUT = ROOT / 'out'
GAME = Path(r'C:\Users\Johnn\AppData\Local\Programs\NX2navi\game')
CACHE = GAME / 'build-cache'
MSYS = Path(r'C:\Users\Johnn\AppData\Local\Programs\NX2navi\toolchain\msys64\ucrt64')
LINK_LIBS = ['-lvulkan-1', '-lSDL3', '-lopus', '-lvpx', '-l:libdav1d.a', '-lzstd', '-l:libatomic.a', '-lbcrypt',
             '-limm32', '-luuid', '-lole32', '-luser32', '-lshell32', '-lgdi32', '-ladvapi32', '-lwinmm']
ENV = dict(os.environ, PATH=str(MSYS / 'bin') + os.pathsep + os.environ.get('PATH', ''))
INC = ['-I', str(RT), '-I', str(CACHE / 'lift'), '-I', str(CACHE / 'gen'), '-I', str(RT / 'third_party' / 'imgui'),
       '-I', str(MSYS / 'include' / 'vma')]


def sources():
    out = []
    for pat in ('*.c', '*.cpp', 'platform/*.c', 'ui/*.cpp', 'third_party/imgui/*.cpp', 'third_party/imgui/backends/*.cpp'):
        for p in sorted(RT.glob(pat)):
            if p.stem.endswith('_sdl') or p.name == 'linux.c':
                continue
            out.append((p, OBJ / ('rt_' + p.relative_to(RT).as_posix().replace('/', '_').rsplit('.', 1)[0] + '.o')))
    return out


def stale(src, obj):
    dep = obj.with_suffix('.d')
    if not obj.is_file() or not dep.is_file():
        return True
    t = obj.stat().st_mtime
    text = dep.read_text(encoding='utf-8', errors='replace').replace('\\\n', ' ')
    parts = re.findall(r'(?:\\ |[^\s])+', text.split(':', 1)[1] if ': ' in text else text)
    for raw in parts:
        f = Path(raw.replace('\\ ', ' '))
        try:
            if f.stat().st_mtime > t:
                return True
        except OSError:
            continue
    return False


def compile_one(job):
    src, dst = job
    cpp = src.suffix in ('.cpp', '.cxx')
    cmd = [str(MSYS / 'bin' / ('clang++.exe' if cpp else 'clang.exe')), '-c', '-std=gnu++17' if cpp else '-std=gnu11', '-O2',
           *RT_MARCH, '-fno-strict-aliasing', '-w', '-MMD', '-MF', str(dst.with_suffix('.d')), *INC, '-o', str(dst), str(src)]
    p = subprocess.run(cmd, capture_output=True, text=True, env=ENV, errors='replace',
                       creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    return src, p.returncode, p.stderr


def resources():
    """runtime/nx2navi.rc compiled as tools/make_game.py does: from runtime/, with gcc as windres's preprocessor."""
    o = OBJ / 'nx2navi_rc.o'
    cmd = [str(MSYS / 'bin' / 'windres.exe'), '--use-temp-file', f'--preprocessor={MSYS / "bin" / "gcc.exe"}',
           '--preprocessor-arg=-E', '--preprocessor-arg=-xc', '--preprocessor-arg=-DRC_INVOKED',
           '-O', 'coff', '-i', 'nx2navi.rc', '-o', str(o)]
    p = subprocess.run(cmd, cwd=RT, capture_output=True, text=True, env=ENV, errors='replace')
    if p.returncode:
        sys.exit('windres failed: ' + p.stderr[-800:])
    return o


def junction(link, target):
    if not link.exists():
        subprocess.run(['cmd', '/c', 'mklink', '/J', str(link), str(target)], check=True, capture_output=True)


def setup():
    OUT.mkdir(exist_ok=True)
    for name in ('romfs', 'exefs', 'mods', 'shader-cache'):
        junction(OUT / name, GAME / name)
    for f in GAME.iterdir():
        if f.is_file() and f.name not in ('ocarina.exe',) and not f.name.startswith('nx2navi.ini.bak'):
            if not (OUT / f.name).exists():
                shutil.copy2(f, OUT / f.name)
    print('out folder ready')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--jobs', type=int, default=8)
    ap.add_argument('--setup', action='store_true')
    ap.add_argument('--guest', help='link the game objects of this folder under nx2navi-src (build_guest.py)')
    ap.add_argument('--out', default='ocarina.exe')
    ap.add_argument('--march', help='CPU level for the runtime objects, kept in obj-<level>')
    a = ap.parse_args()
    global OBJ, RT_MARCH
    if a.march:
        OBJ = ROOT / f'obj-{a.march}'
        RT_MARCH = [f'-march={a.march}']
    OBJ.mkdir(exist_ok=True)
    if a.setup:
        setup()
    srcs = sources()
    todo = [j for j in srcs if stale(*j)]
    t0 = time.time()
    print(f'{len(todo)} of {len(srcs)} runtime files to compile', flush=True)
    bad = []
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        for src, rc, err in ex.map(compile_one, todo):
            if rc:
                bad.append((src, err))
    if bad:
        for src, err in bad[:4]:
            print('---', src, '\n', err[:1800])
        sys.exit(f'{len(bad)} files failed')
    mine = {o.name: o for _, o in srcs}
    # every runtime object of this source (a newer source adds files), then the game objects of the install
    lines = ['"%s"' % o.as_posix() for o in mine.values()]
    for line in (CACHE / 'link.rsp').read_text(encoding='utf-8').splitlines():
        name = line.strip().strip('"').rsplit('/', 1)[-1]
        if not name.startswith('rt_'):
            if name == 'nx2navi_rc.o':   # our icon and manifest, not the install's
                lines.append('"%s"' % resources().as_posix())
                continue
            g = ROOT / a.guest / name if a.guest else None
            lines.append('"%s"' % g.as_posix() if g and g.is_file() else line)
    rsp = OBJ / 'link.rsp'
    rsp.write_text('\n'.join(lines), encoding='utf-8')
    tmp = OUT / 'ocarina.exe.new'
    p = subprocess.run([str(MSYS / 'bin' / 'clang++.exe'), '-o', str(tmp), '@' + str(rsp), '-L', str(MSYS / 'lib'), *LINK_LIBS, '-Wl,-Map=' + str(OBJ / 'ocarina.map')],
                       capture_output=True, text=True, env=ENV, errors='replace')
    if p.returncode:
        print(p.stderr[-3000:])
        sys.exit('link failed')
    os.replace(tmp, OUT / a.out)
    print(f'ok: {a.out} {(OUT / a.out).stat().st_size / 1e6:.0f} MB, {time.time() - t0:.0f} s')


main()
