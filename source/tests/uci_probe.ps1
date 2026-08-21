# Drive a UCI engine: send commands, wait for "bestmove", print everything it said.
# Usage: uci_probe.ps1 -Exe <path> -Commands "position startpos","go depth 13" [-TimeoutSec 120]
param(
    [Parameter(Mandatory=$true)][string]$Exe,
    [Parameter(Mandatory=$true)][string[]]$Commands,
    [int]$TimeoutSec = 120,
    [int]$Tail = 0
)

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = (Resolve-Path $Exe).Path
$psi.RedirectStandardInput = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true

$proc = [System.Diagnostics.Process]::Start($psi)

$lines = New-Object System.Collections.Generic.List[string]

$proc.StandardInput.WriteLine("uci")
$proc.StandardInput.Flush()
# drain until uciok
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline) {
    $l = $proc.StandardOutput.ReadLine()
    if ($null -eq $l) { break }
    if ($l -match '^uciok') { break }
}

foreach ($c in $Commands) {
    $proc.StandardInput.WriteLine($c)
    $proc.StandardInput.Flush()
}

# Read until bestmove
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline) {
    if ($proc.HasExited -and $proc.StandardOutput.EndOfStream) { break }
    $l = $proc.StandardOutput.ReadLine()
    if ($null -eq $l) { break }
    $lines.Add($l) | Out-Null
    if ($l -match '^bestmove') { break }
}

try { $proc.StandardInput.WriteLine("quit"); $proc.StandardInput.Flush() } catch {}
if (-not $proc.WaitForExit(3000)) { try { $proc.Kill() } catch {} }

if ($Tail -gt 0) { $lines | Select-Object -Last $Tail } else { $lines }
