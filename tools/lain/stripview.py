# stripview.py <out.png> <img> [<img> ...]: each image stretched, with its left and right 12% strips enlarged and contrast-stretched below.
import sys
from PIL import Image, ImageOps
ims=[Image.open(p).convert('RGB') for p in sys.argv[2:]]
W=Image.new('RGB',(len(ims)*642,360+620),'white')
for i,im in enumerate(ims):
    w,h=im.size
    W.paste(ImageOps.autocontrast(im,cutoff=.2).resize((640,360)),(i*642,0))
    s=int(w*.12)
    L=ImageOps.autocontrast(im.crop((0,0,s,h)),cutoff=.2).resize((318,600))
    R=ImageOps.autocontrast(im.crop((w-s,0,w,h)),cutoff=.2).resize((318,600))
    W.paste(L,(i*642,370)); W.paste(R,(i*642+322,370))
W.save(sys.argv[1])
