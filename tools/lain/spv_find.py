# spv_find.py <dumped sh_*.spv> ...: the translated shader-cache file with the same bytes, and its input id (NX2NAVI_SPV_IID name).
import hashlib, os, struct, sys
from pathlib import Path
GAME = Path(r'C:\Users\Johnn\AppData\Local\Programs\NX2navi\game')
base = bytes.fromhex((GAME / 'nx2navi_xlate.id').read_text().strip()[:64])
xlate = hashlib.sha256(base + b'TKHO f64=1 sbuf=0 sg=64,32,32 gather=8 zero_locals bda').digest()
sigs = {}
for p in sys.argv[1:]:
    b = open(p, 'rb').read(); sigs.setdefault(len(b), []).append((p, b))
keys = {}
for e in os.scandir(GAME / 'shader-cache' / 'in'):
    try: iid = bytes.fromhex(e.name[:64])
    except ValueError: continue
    keys[hashlib.sha256(b'TKSK' + struct.pack('<I', 5) + xlate + iid).hexdigest()] = e.name[:16]
for k, iid in keys.items():
    f = GAME / 'shader-cache' / (k + '.spv')
    try: n = f.stat().st_size
    except OSError: continue
    if n in sigs:
        b = f.read_bytes()
        for p, s in sigs[n]:
            if s == b: print(os.path.basename(p), 'iid', iid, 'spv', k)
print('done, sizes', list(sigs))
