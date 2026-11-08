# fsdbg.py <dir> <r> <g> <b>: fs 560e with H1 rgb replaced by three of its float variables (SPIR-V ids), alpha 1.
import sys, os
d, r, g, b = sys.argv[1:5]
s = open('shaders/m_560e.spvasm', encoding='utf-8').read().replace('\r\n', '\n')
k = '         %81 = OpFunction %79 None %80\n'
assert s.count(k) == 1
s = s.replace(k, '%90002 = OpConstant %4 1\n' + k)
i = s.index('%8020 = OpAccessChain %1238 %76 %1205')
j = s.index('OpReturn', i)
s = s[:j] + f'''%90010 = OpLoad %4 %{r}
%90011 = OpLoad %4 %{g}
%90012 = OpLoad %4 %{b}
OpStore %7993 %90010
OpStore %8002 %90011
OpStore %8011 %90012
OpStore %8020 %90002
''' + s[j:]
os.makedirs(d, exist_ok=True)
open(os.path.join(d, 'e.spvasm'), 'w', encoding='utf-8', newline='\n').write(s)
