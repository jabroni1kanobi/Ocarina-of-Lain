# edgefind.py <png> ...: where a long straight vertical or horizontal edge lies: columns/rows with the largest count of pixels whose hue step is large.
import sys
import numpy as np
from PIL import Image
for p in sys.argv[1:]:
    a=np.array(Image.open(p).convert('RGB'),dtype=float)
    h,w,_=a.shape
    c=a[:,:,2]-a[:,:,0]          # blue minus red: the box is a colour step
    gx=np.abs(c[:,2:]-c[:,:-2]); gy=np.abs(c[2:,:]-c[:-2,:])
    t=max(3,np.percentile(gx,97))
    cols=(gx>t).sum(0); rows=(gy>t).sum(1)
    tc=np.argsort(cols)[-8:][::-1]; tr=np.argsort(rows)[-8:][::-1]
    print(p.split('/')[-1][:44],w,h,'thr %.0f'%t)
    print('  cols',[(int(x+1),int(cols[x])) for x in tc]); print('  rows',[(int(y+1),int(rows[y])) for y in tr])
