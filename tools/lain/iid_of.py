# iid_of.py <translated spv sha prefix> ...: the input id whose translation is that .spv (the name NX2NAVI_SPV_IID wants)
import hashlib, os, struct, sys
from pathlib import Path
GAME = Path(r'C:\Users\Johnn\AppData\Local\Programs\NX2navi\game')
base = bytes.fromhex((GAME / 'nx2navi_xlate.id').read_text().strip()[:64])
xlate = hashlib.sha256(base + b'TKHO f64=1 sbuf=0 sg=64,32,32 gather=8 zero_locals bda').digest()
want = set(sys.argv[1:])
for e in os.scandir(GAME / 'shader-cache' / 'in'):
    try: iid = bytes.fromhex(e.name[:64])
    except ValueError: continue
    k = hashlib.sha256(b'TKSK' + struct.pack('<I', 5) + xlate + iid).hexdigest()
    for w in want:
        if k.startswith(w): print(w, '->', e.name[:16], e.name)
