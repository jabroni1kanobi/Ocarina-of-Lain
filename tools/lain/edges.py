# edges.py <shots dir> <out png>: left/right edge strips and small full views of the last shots, contrast-stretched.
import glob, os, sys
from PIL import Image, ImageOps

fs = sorted(glob.glob(os.path.join(sys.argv[1], 'shot_*.png')), key=os.path.getmtime)[-6:-1]
W = Image.new('RGB', (5 * 392, 640 + 230), 'white')
for i, n in enumerate(fs):
    im = Image.open(n).convert('RGB')
    L = im.crop((26, 600, 226, 1400))
    R = im.crop((2334, 600, 2534, 1400))
    c = Image.new('RGB', (404, 800), 'white')
    c.paste(L, (0, 0))
    c.paste(R, (204, 0))
    W.paste(ImageOps.autocontrast(c, cutoff=.5).resize((388, 640)), (i * 392, 0))
    W.paste(im.resize((388, 214)), (i * 392, 650))
W.save(sys.argv[2])
Image.open(fs[-2]).convert('RGB').resize((1280, 705), Image.LANCZOS).save(sys.argv[2].replace('.png', '_full.png'))
