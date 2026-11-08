"""Compile the install's lifted game code (read-only) into guest-<level>/, at a CPU level the 0.5.0 kit did not use.
Flags as tools/make_game.py step_compile, with -march. Prints how long it took; build_rt.py --guest <level> links them.
    python build_guest.py [--level x86-64-v3] [--opt -O1] [--jobs 8]
"""
import argparse, concurrent.futures, os, subprocess, sys, time
from pathlib import Path

ROOT = Path(r'C:\Users\Johnn\Games\nx2navi-src')
RT = ROOT / 'src' / 'runtime'
CACHE = Path(r'C:\Users\Johnn\AppData\Local\Programs\Ocarina of Lain\game\build-cache')
MSYS = Path(r'C:\Users\Johnn\AppData\Local\Programs\Ocarina of Lain\toolchain\msys64\ucrt64')
ENV = dict(os.environ, PATH=str(MSYS / 'bin') + os.pathsep + os.environ.get('PATH', ''))
lift = CACHE / 'lift'
INC = ['-I', str(RT), '-I', str(lift), '-I', str(CACHE / 'gen'), '-I', str(RT / 'third_party' / 'imgui'), '-I', str(MSYS / 'include' / 'vma')]

ap = argparse.ArgumentParser()
ap.add_argument('--level', default='x86-64-v3')
ap.add_argument('--opt', default='-O1')
ap.add_argument('--jobs', type=int, default=8)
a = ap.parse_args()
out = ROOT / f'guest-{a.level}{a.opt}'
out.mkdir(exist_ok=True)
jobs = [(p, out / f'{m}_{p.stem}.o') for m in ('main', 'sdk', 'subsdk0') for p in sorted((lift / m).glob('*.c'))]
jobs = [j for j in jobs if not j[1].is_file() or j[1].stat().st_mtime < j[0].stat().st_mtime]
print(f'{len(jobs)} files', flush=True)


def one(j):
    src, dst = j
    cmd = [str(MSYS / 'bin' / 'clang.exe'), '-c', '-std=gnu11', a.opt, f'-march={a.level}', '-fno-strict-aliasing', '-w', *INC, '-o', str(dst), str(src)]
    p = subprocess.run(cmd, capture_output=True, text=True, env=ENV, errors='replace', creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    return src, p.returncode, p.stderr


t0 = time.time()
bad = []
with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
    for i, (src, rc, err) in enumerate(ex.map(one, jobs), 1):
        if rc:
            bad.append((src, err))
        if i % 100 == 0:
            print(f'{i} done, {time.time() - t0:.0f} s', flush=True)
if bad:
    print(bad[0][0], bad[0][1][:1500])
    sys.exit(f'{len(bad)} failed')
print(f'ok {time.time() - t0:.0f} s; {sum(f.stat().st_size for f in out.glob("*.o")) / 1e6:.0f} MB in {out}')
