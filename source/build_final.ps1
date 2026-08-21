# Build the deliverable with profile-guided optimisation, then verify it.
#
#   1. build an instrumented binary
#   2. run a representative search workload to collect a profile
#   3. rebuild with the profile, statically linked
#   4. verify: standalone, perft-correct, UCI-compliant, plays a move on the clock
#
# GCC names the .gcda after the *output* path, so the instrumented and final
# builds must be written to the same filename; the verified binary is copied to
# final/ only at the end, so a half-built engine is never left in place.
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
$stage   = "source\build\pgo_stage.exe"
$common  = @('-O3','-march=x86-64-v3','-std=c++20','-DNDEBUG','-fno-math-errno')

New-Item -ItemType Directory -Force -Path $profDir | Out-Null
# GCC mangles the absolute output path into a subdirectory tree under $profDir,
# so the .gcda files are nested rather than sitting directly in it.
Get-ChildItem "$profDir" -Recurse -Filter *.gcda -ErrorAction SilentlyContinue | Remove-Item -Force

Write-Host "[1/4] building instrumented binary..."
& g++ @common "-fprofile-generate=$profDir" '-Wl,--stack,16777216' -o $stage $src
if ($LASTEXITCODE -ne 0) { throw "instrumented build failed" }

Write-Host "[2/4] collecting profile..."
& $stage bench 11 | Out-Null
if ($LASTEXITCODE -ne 0) { throw "profile run failed" }
$gcda = (Get-ChildItem "$profDir" -Recurse -Filter *.gcda -ErrorAction SilentlyContinue | Measure-Object).Count
if ($gcda -eq 0) { throw "no profile data was produced" }
Write-Host "  $gcda profile file(s)"

Write-Host "[3/4] rebuilding with profile, static..."
& g++ @common '-static' "-fprofile-use=$profDir" '-fprofile-correction' '-Wl,--stack,16777216' -o $stage $src 2>&1 |
    Where-Object { $_ -notmatch 'Wmissing-profile' } | Write-Host
if ($LASTEXITCODE -ne 0) { throw "final build failed" }

Write-Host "[4/4] verifying..."
$dlls = (& objdump -x $stage | Select-String -Pattern "DLL Name:" -SimpleMatch) -join "; "
Write-Host "  dependencies: $dlls"
if ($dlls -match "libstdc|libgcc|libwinpthread") { throw "NOT standalone: $dlls" }

if (-not $SkipPerft) {
    $p = & $stage "perftsuite" "resources\perft\perft.epd" "5" | Select-Object -Last 1
    Write-Host "  $p"
    if ($p -notmatch "0 failures") { throw "perft FAILED: $p" }
}

# fastchess writes a harmless 'Failed to get console mode' line to stderr
$ErrorActionPreference = "Continue"
$c = (& ".\resources\fastchess\fastchess.exe" --compliance $stage 2>&1 | Out-String)
$ErrorActionPreference = "Stop"
if ($c -notmatch "passed all compliance") { throw "compliance FAILED" }
Write-Host "  compliance: passed all checks"

$bm = & "source\tests\uci_probe.ps1" -Exe $stage -Commands "position startpos","go wtime 10000 btime 10000 winc 100 binc 100" -Tail 1
Write-Host "  10+0.1 first move: $bm"
if ($bm -notmatch "^bestmove [a-h][1-8][a-h][1-8]") { throw "no legal bestmove" }

# Only now replace the deliverable
Copy-Item $stage $Out -Force
Write-Host "OK -> $Out"
Get-Item $Out | Select-Object Name, Length, LastWriteTime
