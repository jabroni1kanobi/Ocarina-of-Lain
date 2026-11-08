# Run NX2navi on a scratch profile under the RGP capture CLI; send 'c' when the log reaches given frames.
# rgp_run.ps1 -Name kok1 -Script kokiri_white.txt -Profile kokiri -CaptureAt 2880,3960 -StopAt 4200
param(
  [string]$Name, [string]$Script, [string]$Profile = 'kokiri', [int[]]$CaptureAt = @(), [int]$StopAt = 4200,
  [string]$Exe = 'C:\Users\Johnn\AppData\Local\Programs\NX2navi\game\ocarina.exe',
  [switch]$NoRgp, [int]$FlightAt = 0, [hashtable]$Extra = @{}, [string]$CliExtra = '', [int]$TimeoutS = 400
)
$tools = 'C:\Users\Johnn\Games\nx2navi-tools'
$rdts = "$tools\rdts\RadeonDeveloperToolSuite-2026-10-02-1872"
$out = "$tools\runs\$Name"; New-Item -ItemType Directory -Force $out | Out-Null

if (Get-Process ocarina -ErrorAction SilentlyContinue) { throw 'ocarina is running: not starting (owner game?)' }
$free = (Get-PSDrive C).Free / 1GB
if ($free -lt 34) { throw "C: free $([int]$free) GB: under the floor margin" }
$os = Get-CimInstance Win32_OperatingSystem
if ($os.FreeVirtualMemory / 1MB -lt 14) { throw 'commit free under 14 GB' }

$prof = "$out\profile"
if (Test-Path $prof) { Remove-Item -Recurse -Force $prof }
if ($Profile -eq 'kokiri') {
  Copy-Item -Recurse 'C:\Users\Johnn\Downloads\ocarina-of-time-recomp-main\build\profile-kokiri-master' $prof
} else { New-Item -ItemType Directory -Force $prof | Out-Null }

$cli = $null
if (-not $NoRgp) {
  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName = "$rdts\RadeonDeveloperPanelCLI.exe"
  $psi.WorkingDirectory = $rdts
  $psi.Arguments = "-m profiling -p ocarina -o `"$out\cap.rgp`" --rgp-sqtt-buffer-size high --verbose $CliExtra"
  $psi.UseShellExecute = $false; $psi.RedirectStandardInput = $true
  $psi.RedirectStandardOutput = $true; $psi.RedirectStandardError = $true; $psi.CreateNoWindow = $true
  $cli = New-Object System.Diagnostics.Process; $cli.StartInfo = $psi
  $outLog = New-Object System.Text.StringBuilder
  Register-ObjectEvent $cli OutputDataReceived -MessageData $outLog -Action { if ($EventArgs.Data) { [void]$Event.MessageData.AppendLine($EventArgs.Data) } } | Out-Null
  Register-ObjectEvent $cli ErrorDataReceived -MessageData $outLog -Action { if ($EventArgs.Data) { [void]$Event.MessageData.AppendLine($EventArgs.Data) } } | Out-Null
  [void]$cli.Start(); $cli.BeginOutputReadLine(); $cli.BeginErrorReadLine()
  Start-Sleep 3
}

$env:NX2NAVI_PROFILE_DIR = $prof
$env:NX2NAVI_INPUT_SCRIPT = "$tools\$Script"
$env:NX2NAVI_TRACE = 'perf'
$env:NX2NAVI_FPS_LIMIT = '60'
$env:NX2NAVI_ASSUME_FOCUS = '1'
$env:NX2NAVI_BACKGROUND_WINDOW = '1'
$env:NX2NAVI_INPUT = 'none'   # the owner's pad, keyboard and mouse must not reach a test window; scripts still apply
if ($FlightAt) { $env:NX2NAVI_FLIGHT = '1'; $env:NX2NAVI_FLIGHT_AT = "$FlightAt"; $env:NX2NAVI_FLIGHT_DIR = "$out\flight" }
foreach ($k in $Extra.Keys) { Set-Item "env:$k" $Extra[$k] }

$log = "$out\game.log"
$game = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -PassThru -RedirectStandardError $log -RedirectStandardOutput "$out\game.out"
$pending = [System.Collections.ArrayList]@($CaptureAt | Sort-Object)
$t0 = Get-Date; $last = 0; $reason = 'timeout'
while (((Get-Date) - $t0).TotalSeconds -lt $TimeoutS) {
  Start-Sleep -Milliseconds 500
  if ($game.HasExited) { $reason = "exited $($game.ExitCode)"; break }
  $dfree = (Get-PSDrive C).Free / 1GB
  if ($null -eq $script:minDisk -or $dfree -lt $script:minDisk) { $script:minDisk = $dfree }
  # The owner allowed dips to about 24 GB for these runs (2026-11-08): the page file jumps about 19 GB and comes back.
  if ($dfree -lt 22) { $reason = "disk floor: C: free $([math]::Round($dfree,1)) GB at frame $last"; break }
  $cf = (Get-CimInstance Win32_OperatingSystem).FreeVirtualMemory / 1MB
  if ($null -eq $script:minCommit -or $cf -lt $script:minCommit) { $script:minCommit = $cf }
  $tail = Get-Content -LiteralPath $log -Tail 200 -ErrorAction SilentlyContinue
  $m = $tail | Select-String -Pattern 'perf: frame (\d+): interval' | Select-Object -Last 1
  if ($m) { $last = [int]$m.Matches[0].Groups[1].Value }
  while ($pending.Count -and $last -ge $pending[0]) {
    if ($cli) { $cli.StandardInput.WriteLine('c'); $cli.StandardInput.Flush() }
    "capture requested at frame $last ($(((Get-Date)-$t0).TotalSeconds.ToString('0.0')) s)"
    $pending.RemoveAt(0); Start-Sleep 12
  }
  if ($last -ge $StopAt) { $reason = "reached frame $last"; break }
}
"stop: $reason after $(((Get-Date)-$t0).TotalSeconds.ToString('0')) s; lowest commit free {0:N1} GB, lowest C: free {1:N1} GB" -f $script:minCommit, $script:minDisk
if (-not $game.HasExited) { Stop-Process -Id $game.Id -Force }
if ($cli) {
  Start-Sleep 2
  try { $cli.StandardInput.WriteLine('q'); $cli.StandardInput.Flush() } catch {}
  if (-not $cli.WaitForExit(8000)) { $cli.Kill() }
  Start-Sleep 1
  $outLog.ToString() | Set-Content -LiteralPath "$out\cli.log" -Encoding utf8
}
foreach ($k in $Extra.Keys) { Remove-Item "env:$k" -ErrorAction SilentlyContinue }   # a batch's next run must not inherit them
Remove-Item -Recurse -Force $prof -ErrorAction SilentlyContinue
Get-ChildItem $out -File | Select-Object Name, Length | Format-Table -AutoSize | Out-String
"C: free GB: {0:N1}" -f ((Get-PSDrive C).Free / 1GB)
