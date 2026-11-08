# box_batch.ps1 -Runs 'name|key=val;key=val','name2|...': one Saria-scene run after another, shots on, waits for C: to recover between runs.
param([string[]]$Runs, [int]$StopAt = 11000, [int]$TimeoutS = 340, [int]$After = 150, [int]$Every = 8, [string]$Script = 'kokiri_white.txt')
$tools = 'C:\Users\Johnn\Games\nx2navi-tools'; $py = 'C:/Users/Johnn/AppData/Local/Programs/Python/Python312/python.exe'
$exe = 'C:\Users\Johnn\Games\nx2navi-src\out\ocarina.exe'
foreach ($r in $Runs) {
  $p = $r -split '\|', 2; $n = $p[0]; $o = "$tools\runs\$n"
  if (Test-Path $o) { Remove-Item -Recurse -Force $o }
  $e = @{ NX2NAVI_SHOT_EVERY = "$Every"; NX2NAVI_SHOT_AFTER = "$After"; NX2NAVI_SHOT_DIR = "$o\shots" }
  New-Item -ItemType Directory -Force "$o\shots" | Out-Null
  if ($p.Count -gt 1 -and $p[1]) { foreach ($kv in $p[1] -split ';') { $x = $kv -split '=', 2; $e[$x[0]] = $x[1] } }
  $t0 = Get-Date
  while ((((Get-PSDrive C).Free / 1GB) -lt 42.8) -and ((Get-Date) - $t0).TotalSeconds -lt 600) { Start-Sleep 10 }
  while ((Get-Process ocarina -ErrorAction SilentlyContinue) -and ((Get-Date) - $t0).TotalSeconds -lt 600) { Start-Sleep 5 }
  "== $n  $($p[1])"
  & "$tools\rgp_run.ps1" -Name $n -Script $Script -NoRgp -StopAt $StopAt -TimeoutS $TimeoutS -Exe $exe -Extra $e | Select-String -Pattern 'stop:|C: free'
  & $py -I "$tools\metrics.py" "$o\game.log" 9000 10900
  $bad = Select-String -LiteralPath "$o\game.log" -Pattern 'FATAL|device fault' | Select-Object -First 1
  if ($bad) { "**FAULT** $bad" }
  & $py -I "$tools\edges.py" "$o\shots" "$tools\$($n)_edges.png"
  Start-Sleep 12
}
"BATCH DONE"
