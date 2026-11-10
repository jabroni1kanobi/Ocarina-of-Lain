"""Write lain/menu-bg.rgba, the settings menu's background (runtime/ui/menu_art.cpp menu_lain_bg), from any picture.

    python make_menu_bg.py <picture> <game folder>

The picture is scaled to fit 1920x1080 at most and stored raw: "LAIN", width and height (little-endian 32-bit), then
the RGBA bytes. It stays out of the repository: use a picture you may use.
"""
import struct
import sys
from pathlib import Path

from PIL import Image

src, game = Path(sys.argv[1]), Path(sys.argv[2])
img = Image.open(src).convert('RGBA')
img.thumbnail((1920, 1080), Image.LANCZOS)
out = game / 'lain' / 'menu-bg.rgba'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_bytes(b'LAIN' + struct.pack('<II', *img.size) + img.tobytes())
print(f'{out}: {img.size[0]}x{img.size[1]}')
