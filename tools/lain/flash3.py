import glob,os,sys,numpy as np
from PIL import Image
f={}; empty=0
for p in glob.glob(sys.argv[1]+'/*/f*.png'):
    f.setdefault(int(os.path.basename(p)[1:-4]),p)
bad=0; ns=sorted(f); prev=None; runs=[]; cur=None; ms=[]
L={}
def g(n):
    if n not in L:
        a=np.asarray(Image.open(f[n]).convert('L'),dtype=np.int16).copy(); a[:24,-60:]=0; L[n]=a
    return L[n]
for i in range(1,len(ns)-1):
    a,b,c=ns[i-1],ns[i],ns[i+1]
    if b!=a+1 or c!=b+1: continue
    try: A,B,C=g(a),g(b),g(c)
    except Exception: bad+=1; continue
    pct=100*((B-A>30)&(B-C>30)).mean(); L.pop(a,None)
    if pct>=0.5:
        if cur and b==cur[1]+1: cur[1]=b; cur[2]=max(cur[2],pct)
        else:
            cur=[b,b,pct]; runs.append(cur)
print(f'{len(ns)} frames, {empty} empty files {ns[0] if ns else 0}-{ns[-1] if ns else 0}; spike runs: {len(runs)}', f'unreadable {bad};', ' '.join(f'{r[0]}-{r[1]}({r[1]-r[0]+1}f,{r[2]:.0f}%)' for r in runs[:25]))
