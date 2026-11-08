"""Frame-time headroom from a run's log: python -I metrics.py <game.log> <first frame> <last frame>
Means over the perf windows in the range; a capped run shows its headroom as pacing and as render busy minus pacing."""
import re, sys, statistics as st
log, lo, hi = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
rows = {}
pat = {
    'interval': r'interval avg ([0-9.]+)', 'maxf': r'interval avg [0-9.]+ ms max ([0-9.]+)', 'pacing': r'pacing (\d+)%', 'fence': r'fence (\d+)%',
    'busy': r'render busy avg ([0-9.]+)', 'replay': r'replay avg ([0-9.]+)',
}
gpu = re.compile(r'perf: frame (\d+): gpu avg ([0-9.]+)')
head = re.compile(r'perf: frame (\d+): interval')
ru = re.compile(r'perf: frame (\d+): texture re-uploads (\d+) \(([0-9.]+) MiB, ([0-9.]+) ms\).*?(?:skipped with unchanged bytes (\d+))?\s*(?:\(.*)?$')
mir = re.compile(r'mem: mirror frame (\d+):.*?syncs ([0-9.]+), upload ([0-9.]+) MiB.*?write-back ([0-9.]+) MiB')
for line in open(log, encoding='utf-8', errors='replace'):
    m = head.search(line)
    if m:
        f = int(m.group(1))
        if lo <= f <= hi:
            r = rows.setdefault(f, {})
            for k, p in pat.items():
                x = re.search(p, line)
                if x: r[k] = float(x.group(1))
        continue
    m = gpu.search(line)
    if m and lo <= int(m.group(1)) <= hi: rows.setdefault(int(m.group(1)), {})['gpu'] = float(m.group(2)); continue
    m = ru.search(line)
    if m and lo <= int(m.group(1)) <= hi:
        r = rows.setdefault(int(m.group(1)), {}); r['reup'] = float(m.group(2)); r['reup_ms'] = float(m.group(4)); r['same'] = float(m.group(5) or 0); continue
    m = mir.search(line)
    if m and lo <= int(m.group(1)) <= hi:
        r = rows.setdefault(int(m.group(1)), {}); r['syncs'] = float(m.group(2)); r['up_mib'] = float(m.group(3)); r['wb_mib'] = float(m.group(4))
keys = ('interval', 'maxf', 'pacing', 'fence', 'busy', 'replay', 'gpu', 'reup', 'reup_ms', 'same', 'syncs', 'up_mib', 'wb_mib')
print(f'{len(rows)} windows {lo}-{hi}: ' + '  '.join(f'{k} {st.mean(r[k] for r in rows.values() if k in r):.2f}' for k in keys if any(k in r for r in rows.values())))
