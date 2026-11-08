# Alternating A/B: ab.ps1 -Tag x -Reps 3 -Script kokiri_white.txt -From 3420 -To 3720 -Stop 3720 -Conds @(@{n='a';exe='...';env=@{}}, ...)
param([string]$Tag, [int]$Reps = 3, [string]$Script = 'kokiri_white.txt', [string]$Prof = 'kokiri', [int]$From = 3420, [int]$To = 3720, [int]$Stop = 3720, [array]$Conds)
$tools = 'C:\Users\Johnn\Games\nx2navi-tools'; $py = 'C:/Users/Johnn/AppData/Local/Programs/Python/Python312/python.exe'
$res = @{}
for ($i = 1; $i -le $Reps; $i++) {
  foreach ($c in $Conds) {
    $n = "$Tag-$($c.n)-$i"; $o = "$tools\runs\$n"
    if (Test-Path $o) { Remove-Item -Recurse -Force $o }
    if ($c.swap) { $x = Join-Path (Split-Path $c.exe) 'ocarina.exe'; if (Test-Path $x) { Remove-Item -LiteralPath $x -Force }; New-Item -ItemType HardLink -Path $x -Target $c.swap | Out-Null }
    $t0 = Get-Date; while (((Get-PSDrive C).Free / 1GB -lt 42.8) -and ((Get-Date) - $t0).TotalSeconds -lt 300) { Start-Sleep 10 }
    $r = & "$tools\rgp_run.ps1" -Name $n -Script $Script -Profile $Prof -NoRgp -StopAt $Stop -TimeoutS 420 -Exe $c.exe -Extra $c.env
    $m = & $py -I "$tools\metrics.py" "$o\game.log" $From $To
    $bad = Select-String -LiteralPath "$o\game.log" -Pattern 'FATAL|device fault' | Select-Object -First 1
    "$n  $m" + $(if ($bad) { '  **FAULT**' })
    if (-not $res[$c.n]) { $res[$c.n] = @() }
    $res[$c.n] += , $m
    Start-Sleep 12
  }
}
"--- medians"
foreach ($c in $Conds) {
  $line = "$($c.n):"
  foreach ($k in 'interval', 'busy', 'replay', 'gpu', 'maxf', 'up_mib', 'wb_mib') {
    $v = $res[$c.n] | ForEach-Object { $x = [regex]::Match($_, "\b$k ([0-9.]+)"); if ($x.Success) { [double]$x.Groups[1].Value } } | Sort-Object
    if ($v) { $line += " $k {0:N2}" -f @($v)[[int][math]::Floor(@($v).Count / 2)] }
    if ($k -eq 'replay' -and $v) { $line += " (range {0:N2}-{1:N2})" -f @($v)[0], @($v)[-1] }
  }
  $line
}

