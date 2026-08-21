# SPRT a candidate build against a baseline build at the final time control.
# Usage: sprt.ps1 -New build\new.exe -Old build\old.exe [-Elo0 0] [-Elo1 8] [-Rounds 4000] [-TC 10+0.1]
param(
    [Parameter(Mandatory=$true)][string]$New,
    [Parameter(Mandatory=$true)][string]$Old,
    [double]$Elo0 = 0,
    [double]$Elo1 = 8,
    [int]$Rounds = 4000,
    [string]$TC = "10+0.1",
    [int]$Concurrency = 10,
    [string]$Tag = "sprt"
)

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root

$fcArgs = @(
  '-engine', "cmd=$New", 'name=new',
  '-engine', "cmd=$Old", 'name=old',
  '-each', "tc=$TC", 'option.Hash=256',
  '-openings', 'file=resources\fastchess\UHO.pgn', 'format=pgn', 'order=random', 'plies=16',
  '-sprt', "elo0=$Elo0", "elo1=$Elo1", 'alpha=0.05', 'beta=0.05',
  '-rounds', "$Rounds", '-repeat', '-concurrency', "$Concurrency",
  '-ratinginterval', '50', '-recover',
  '-pgnout', "file=source\tests\$Tag.pgn"
)

& ".\resources\fastchess\fastchess.exe" @fcArgs 2>&1 | Tee-Object -FilePath "source\tests\$Tag.log"
