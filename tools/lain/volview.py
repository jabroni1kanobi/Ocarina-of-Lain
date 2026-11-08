# volview.py <out.png> <img:w:h:d> ...: slices of side-by-side 3D dumps, each row one volume, each slice contrast-stretched over the whole volume.
import sys
from PIL import Image, ImageOps
rows=sys.argv[2:]; Z=[1,3,6,12,24,48,90]
W=Image.new('RGB',(len(Z)*(256+4),len(rows)*(144+4)),'white')
for r,a in enumerate(rows):
    p,w,h,d=a.rsplit(':',3); w,h,d=int(w),int(h),int(d)
    im=Image.open(p).convert('RGB'); im=ImageOps.autocontrast(im,cutoff=0)
    for i,z in enumerate(Z):
        z=min(z,d-1)
        W.paste(im.crop((z*w,0,(z+1)*w,h)).resize((256,144),Image.NEAREST),(i*260,r*148))
W.save(sys.argv[1])
