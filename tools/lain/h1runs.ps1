# h1runs.ps1 -Names ova,ovb: one run each to frame 9700 with the override dir of that name, dumping pass 178.
param([string[]]$Names, [string]$Only = '178', [int]$StopAt = 9700)
$t = 'C:\Users\Johnn\Games\nx2navi-tools'
foreach ($n in $Names) {
  $r = "box-$n"; $o = "$t\runs\$r"
  if (Test-Path $o) { Remove-Item -Recurse -Force $o }
  New-Item -ItemType Directory -Force "$o\d" | Out-Null
  $t0 = Get-Date
  while ((((Get-PSDrive C).Free / 1GB) -lt 42.8) -and ((Get-Date) - $t0).TotalSeconds -lt 300) { Start-Sleep 10 }
  while ((Get-Process ocarina -ErrorAction SilentlyContinue) -and ((Get-Date) - $t0).TotalSeconds -lt 300) { Start-Sleep 5 }
  & "$t\rgp_run.ps1" -Name $r -Script kokiri_white.txt -NoRgp -StopAt $StopAt -TimeoutS 330 -Exe C:\Users\Johnn\Games\nx2navi-src\out\ocarina.exe -Extra @{ NX2NAVI_SPV_IID = "$t\$n"; NX2NAVI_DUMP = '9600'; NX2NAVI_DUMP_DIR = "$o\d"; NX2NAVI_DUMP_ONLY = $Only } | Select-String 'stop:'
  Start-Sleep 12
}
"DONE"
