# Build the deliverable with profile-guided optimisation, then verify it.
#
#   1. build an instrumented binary
#   2. run a representative search workload to collect a profile
#   3. rebuild with the profile, statically linked
#   4. verify: standalone, perft-correct, UCI-compliant, plays a move on the clock
#
# Usage:  source\build_final.ps1 [-Out final\Opus5chess24hrs.exe] [-SkipPerft]
param(
    [string]$Out = "final\Opus5chess24hrs.exe",
    [switch]$SkipPerft
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$src     = "source\src\main.cpp"
$profDir = "source\build\pgo"
$common  = @('-O3','-march=x86-64-v3','-std=c++20','-DNDEBUG','-fno-math-errno')

New-Item -ItemType Directory -Force -Path $profDir | Out-Null
Get-ChildItem "$profDir\*.gcda" -ErrorAction SilentlyContinue | Remove-Item -Force

Write-Host "[1/4] building instrumented binary..."
& g++ @common "-fprofile-generate=$profDir" '-Wl,--stack,16777216' -o "source\build\pgogen.exe" $src
if ($LASTEXITCODE -ne 0) { throw "instrumented build failed" }

Write-Host "[2/4] collecting profile (bench)..."
& "source\build\pgogen.exe" bench 13 | Out-Null
if ($LASTEXITCODE -ne 0) { throw "profile run failed" }

Write-Host "[3/4] rebuilding with profile, static..."
& g++ @common '-static' "-fprofile-use=$profDir" '-fprofile-correction' '-Wl,--stack,16777216' -o $Out $src
if ($LASTEXITCODE -ne 0) { throw "final build failed" }

Write-Host "[4/4] verifying..."
$dlls = (& objdump -x $Out | Select-String -Pattern "DLL Name:" -SimpleMatch) -join "; "
Write-Host "  dependencies: $dlls"
if ($dlls -match "libstdc|libgcc|libwinpthread") { throw "NOT standalone: $dlls" }

if (-not $SkipPerft) {
    Write-Host "  perft suite to depth 5..."
    $p = & $Out "perftsuite" "resources\perft\perft.epd" "5" | Select-Object -Last 1
    Write-Host "  $p"
    if ($p -notmatch "0 failures") { throw "perft FAILED: $p" }
}

$c = & ".\resources\fastchess\fastchess.exe" --compliance $Out 2>&1 | Select-Object -Last 1
Write-Host "  $c"
if ($c -notmatch "passed all compliance") { throw "compliance FAILED" }

$bm = & "source\tests\uci_probe.ps1" -Exe $Out -Commands "position startpos","go wtime 10000 btime 10000 winc 100 binc 100" -Tail 1
Write-Host "  10+0.1 first move: $bm"
if ($bm -notmatch "^bestmove [a-h][1-8][a-h][1-8]") { throw "no legal bestmove" }

Write-Host "OK: $Out"
Get-Item $Out | Select-Object Name, Length, LastWriteTime
