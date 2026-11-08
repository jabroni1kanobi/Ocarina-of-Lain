# fetchclamp_val.py [limit]: every cached shader with an image fetch goes through bin/fetchclamp_test.exe and spirv-val.
import os, struct, subprocess, sys, tempfile
G = r'C:\Users\Johnn\AppData\Local\Programs\NX2navi\game\shader-cache'
V = r'C:\VulkanSDK\1.4.363.0\Bin\spirv-val.exe'; T = r'C:\Users\Johnn\Games\nx2navi-tools\bin\fetchclamp_test.exe'
limit = int(sys.argv[1]) if len(sys.argv) > 1 else 10**9
tmp = tempfile.mktemp(suffix='.spv')
seen = fetch = ok = bad = same = pre = 0; errs = {}
for e in os.scandir(G):
    if not e.name.endswith('.spv'): continue
    seen += 1
    b = open(e.path, 'rb').read()
    if b.find(b'\x5f\x00\x06\x00') < 0 and b.find(b'\x5f\x00\x05\x00') < 0 and b.find(b'\x5f\x00\x07\x00') < 0: continue
    r = subprocess.run([T, e.path, tmp], capture_output=True, text=True)
    add = r.stdout.strip()
    if add == '0': same += 1; continue
    fetch += 1
    v = subprocess.run([V, '--target-env', 'vulkan1.2', tmp], capture_output=True, text=True)
    if v.returncode == 0: ok += 1
    else:
        if subprocess.run([V, '--target-env', 'vulkan1.2', e.path], capture_output=True, text=True).returncode != 0:
            pre += 1; continue
        bad += 1; m = (v.stderr or v.stdout).split('\n')[0][:140]; errs[m[:90]] = errs.get(m[:90], 0) + 1
        if bad <= 3: print('BAD', e.name[:16], m)
    if fetch >= limit: break
print(f'modules seen {seen}, changed {fetch}, valid {ok}, invalid because of the clamp {bad}, invalid before it too {pre}, unchanged {same}')
for k, c in sorted(errs.items(), key=lambda x: -x[1])[:6]: print(c, k)
