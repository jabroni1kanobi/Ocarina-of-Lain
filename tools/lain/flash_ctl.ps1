# flash_ctl.ps1: flash dumps with the fetch clamp on and off, then the Deku route, each with time for the dump to finish.
$t = 'C:\Users\Johnn\Games\nx2navi-tools'; $x = 'C:\Users\Johnn\Games\nx2navi-src\out\ocarina.exe'; $py = 'C:/Users/Johnn/AppData/Local/Programs/Python/Python312/python.exe'
$runs = @(
 @{n='flash-k0';s='kokiri_white.txt';p='kokiri';stop=12000;at='3700,6300,8900';fr='2600';to=420;e=@{NX2NAVI_FETCH_CLAMP='0'}},
 @{n='flash-k1';s='kokiri_white.txt';p='kokiri';stop=12000;at='3700,6300,8900';fr='2600';to=420;e=@{}},
 @{n='flash-d1';s='togame_nodump.txt';p='new';stop=15500;at='12400';fr='1600';to=520;e=@{}})
foreach ($k in $runs) {
  $o = "$t\runs\$($k.n)"; if (Test-Path $o) { Remove-Item -Recurse -Force $o }
  $t0 = Get-Date; while (((Get-PSDrive C).Free / 1GB -lt 42.8) -and ((Get-Date) - $t0).TotalSeconds -lt 300) { Start-Sleep 10 }
  $e = $k.e.Clone(); $e['NX2NAVI_FLIGHT'] = '1'; $e['NX2NAVI_FLIGHT_FRAMES'] = $k.fr; $e['NX2NAVI_FLIGHT_AT'] = $k.at; $e['NX2NAVI_FLIGHT_DIR'] = "$o\flight"; $e['NX2NAVI_FLIGHT_SPIKE_MS'] = '0'
  "== $($k.n)"
  & "$t\rgp_run.ps1" -Name $k.n -Script $k.s -Profile $k.p -NoRgp -StopAt $k.stop -TimeoutS $k.to -Exe $x -Extra $e | Select-String 'stop:'
  Start-Sleep 15
  & $py -I "$t\flash3.py" "$o\flight"
  "fault lines: " + @(Select-String -LiteralPath "$o\game.log" -Pattern 'FATAL|device fault|DEVICE_LOST').Count
}
"DONE"
