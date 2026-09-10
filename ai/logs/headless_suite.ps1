# MosquitoSimulator headless regression suite (MVP 0.2/0.3 plan: every change verified
# without the editor). Runs N -game scenarios, greps log markers, prints PASS/FAIL.
# Usage:  powershell -ExecutionPolicy Bypass -File ai\logs\headless_suite.ps1 [-SkipBuild] [-Only 04]
# Notes:  - aborts if UnrealEditor is running (Live Coding locks DLLs);
#         - backs up and RESTORES your Saved/Config/MosquitoSave.ini around the run.
param(
    [string]$Engine = 'e:\ue_5.8',
    [string]$Project = (Resolve-Path (Join-Path $PSScriptRoot '..\..\MosquitoSimulator.uproject')).Path,
    [switch]$SkipBuild,
    [string]$Only = ''
)
$ErrorActionPreference = 'Stop'
$exe = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$logDir = Join-Path $PSScriptRoot 'suite'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

if (-not (Test-Path $exe)) { Write-Host "engine exe not found: $exe" -ForegroundColor Red; exit 2 }
if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    Write-Host 'Close the Unreal editor first (Live Coding locks module DLLs).' -ForegroundColor Red; exit 2
}

# --- protect the owner's personal save file ---
$save = Join-Path (Split-Path $Project) 'Saved\Config\MosquitoSave.ini'
$backup = Join-Path $env:TEMP 'MosquitoSave_owner_backup.ini'
$hadSave = Test-Path $save
if ($hadSave) { Copy-Item $save $backup -Force }

try {
    if (-not $SkipBuild) {
        Write-Host '== build ==' -ForegroundColor Cyan
        & (Join-Path $Engine 'Engine\Build\BatchFiles\Build.bat') MosquitoSimulatorEditor Win64 Development `
            "-Project=$Project" -WaitMutex *> (Join-Path $logDir 'build.log')
        if ($LASTEXITCODE -ne 0) { Write-Host "BUILD FAILED (see $logDir\build.log)" -ForegroundColor Red; exit 1 }
    }

    $script:results = @()
    function Run-Scenario {
        param([string]$Name, [string[]]$Extra = @(), [int]$Seconds = 0, [hashtable]$Expect, [hashtable]$Forbid = @{})
        if ($Only -and -not $Name.StartsWith($Only)) { return }
        $log = Join-Path $logDir "$Name.log"
        if (Test-Path $log) { Remove-Item $log -Force }
        $argsList = @($Project, '-game', '-nullrhi', '-unattended', '-nosplash', '-nosound') + $Extra
        if ($Seconds -gt 0) { $argsList += @('-benchmark', "-benchmarkseconds=$Seconds") }
        else { $argsList += '-ExecCmds=quit' }
        $argsList += "-abslog=`"$log`""
        $env:GIT_TERMINAL_PROMPT = '0'
        & $exe @argsList 2>&1 | Out-Null
        $code = $LASTEXITCODE
        $fail = @()
        if ($code -ne 0) { $fail += "exit=$code" }
        if (-not (Test-Path $log)) { $fail += 'no log' }
        else {
            foreach ($k in $Expect.Keys) {
                $cnt = @(Select-String -Path $log -Pattern $k -SimpleMatch).Count
                if ($cnt -lt $Expect[$k]) { $fail += "'$k'=$cnt want>=$($Expect[$k])" }
            }
            foreach ($k in $Forbid.Keys) {
                $cnt = @(Select-String -Path $log -Pattern $k -SimpleMatch).Count
                if ($cnt -gt 0) { $fail += "FORBID '$k'=$cnt" }
            }
            $bad = @(Select-String -Path $log -Pattern 'Fatal error|Assertion failed|Handled ensure').Count
            if ($bad -gt 0) { $fail += "fatal/ensure=$bad" }
        }
        $status = if ($fail.Count) { 'FAIL' } else { 'PASS' }
        $script:results += [pscustomobject]@{ Scenario = $Name; Status = $status; Detail = ($fail -join '; ') }
        Write-Host ("{0} {1}" -f $(if ($status -eq 'PASS') { '[PASS]' } else { '[FAIL]' }), $Name) `
            -ForegroundColor $(if ($status -eq 'PASS') { 'Green' } else { 'Red' })
        if ($fail.Count) { Write-Host ("      " + ($fail -join '; ')) -ForegroundColor Red }
    }

    # --- scenarios (order matters: *_read follows its *_write) ---
    Run-Scenario '01_plain' @() 15 @{
        'Blockout village ready' = 1; '[Human] Spawn OK' = 3; '[Spider] Spawn OK' = 1;
        '[Nest] at' = 1; '[Save] Loaded' = 1; '[MosquitoHUD] Active' = 1; 'readback=OK' = 26 }
    Run-Scenario '02_seed_write' @('-WipeSave', '-SeedSave') 0 @{
        '[Save] Wiped' = 1; 'Seeded for headless' = 1; '[Save] Written' = 1 }
    Run-Scenario '03_seed_read' @() 0 @{
        'Loaded version=1 lifetime=123 bestChase=50 deaths=1 generations=1 speciesPts=1' = 1 }
    Run-Scenario '04_spider_trap' @('-SpiderTest') 15 @{
        'TRAPPED in spider web' = 1; '[Spider] Bite! damage' = 1;
        'ESCAPED the web' = 1; 'Test complete - ESCAPED proven (taps=12)' = 1 }
    Run-Scenario '05_spider_kill' @('-KillSpider', '-SpiderRespawn=4') 15 @{
        'Force bite 3/3 counted=yes' = 1; 'Died +reward: web cleared' = 1;
        'Cooldown over - rebuilding the web' = 1 }
    Run-Scenario '06_nest' @('-WipeSave', '-NestTest') 12 @{
        'LIFE COMPLETE - clutch laid' = 1; 'Clutch #1 booked' = 1;
        'NestDev] Test complete: TotalClutches=1' = 1 }
    Run-Scenario '07_nest_read' @() 0 @{
        'clutch=1 lastMut=3' = 1; 'Species inherited at spawn: Exoskeleton +10 HP' = 1 }
    Run-Scenario '08_deathloop' @('-WipeSave', '-SeedScore=420', '-KillMe') 15 @{
        'Species reward +2 point(s)' = 1; 'Written version=1 lifetime=420 speciesPts=2 clutch=0' = 1;
        'Respawned!' = 1 }
    Run-Scenario '09_deathloop_read' @() 0 @{
        'Loaded version=1 lifetime=420 bestChase=0 deaths=1 generations=1 speciesPts=2' = 1 }
    Run-Scenario '10_stat_speed' @('-StatSpeed=100') 15 @{
        'Stat speed x100' = 1; 'Exhausted!' = 1; 'Starving' = 1 }

    # 11: hand-corrupted save (missing version, garbage values) -> defaults, no crash
    # (plan MVP 0.2 §2 verification case, now part of the standing regression).
    Set-Content -Path $save -Value "[MosquitoSave]`r`nLifetimeScore=-999`r`nTotallyBogus=abc`r`n" -Encoding ascii
    Run-Scenario '11_corrupt_defaults' @() 0 @{
        'Version mismatch' = 1; 'Loaded version=1 lifetime=0' = 1; 'Written version=1 lifetime=0' = 1 }

    # 12: run progression - XP curve level-up + purchase API (Tab panel's backend).
    Run-Scenario '12_run_progression' @('-SeedRunXP=250') 0 @{
        'Seeded run XP=250' = 1; 'Level up! RunLevel=2 points=1' = 1;
        'Bought Muscular Propulsion -> L1' = 1 }

    # 13: day/night cycle (0.1 system) - 2s days must cross both event lines.
    Run-Scenario '13_daynight' @('-DayLength=2') 15 @{
        '[DayNight] Started at' = 1; '[DayNight] SUNSET at' = 1; '[DayNight] SUNRISE at' = 1 }

    # 14/15: species-award boundary - exactly 200 lifetime pays +1, 199 pays nothing.
    Run-Scenario '14_award_boundary_200' @('-WipeSave', '-SeedScore=200', '-KillMe') 15 @{
        'Species reward +1 point(s)' = 1; 'Written version=1 lifetime=200 speciesPts=1' = 1 } `
        @{ 'Species reward +2' = 1 }
    Run-Scenario '15_award_below_200' @('-WipeSave', '-SeedScore=199', '-KillMe') 15 @{
        'Written version=1 lifetime=199 speciesPts=0' = 1 } `
        @{ 'Species reward' = 1 }

    # 16: trapped + death edge - respawn must clear the web trap BEFORE input
    # (plan 1 edge case) and apply the soft punishment values (plan 4).
    Run-Scenario '16_trapped_death_respawn' @('-WipeSave', '-SpiderTest', '-KillMe') 15 @{
        'TRAPPED in spider web' = 1;
        'Respawned! blood=20 hunger=70 maxHealth=100 trapped=0' = 1 }

    # 17: multi-web data-driven spawn - two independent spiders, each with its own
    # kill -> per-web cooldown -> rebuild (default single-web behavior stays 01/05).
    Run-Scenario '17_multi_web' @('-WipeSave', '-KillSpider', '-SpiderWebs=2', '-SpiderRespawn=4') 15 @{
        '[Spider] Spawn OK' = 2; 'Died +reward: web cleared' = 2;
        'Force bite 3/3 counted=yes' = 2; 'Cooldown over - rebuilding the web' = 2 }

    # 18: multi-nest data-driven spawn - three nests exist, exactly one clutch fires
    # (HasClutchedThisRun gate is per mosquito run, not per nest).
    Run-Scenario '18_multi_nest' @('-WipeSave', '-NestTest', '-NestCount=3') 12 @{
        '[Nest] at' = 3; 'Clutch #1 booked' = 1;
        'NestDev] Test complete: TotalClutches=1' = 3 }

    Write-Host ''
    $script:results | Format-Table -AutoSize
    $failed = @($script:results | Where-Object Status -eq 'FAIL')
    if ($failed.Count) { Write-Host "SUITE: $($failed.Count) FAILED" -ForegroundColor Red; exit 1 }
    Write-Host 'SUITE: ALL PASS' -ForegroundColor Green
}
finally {
    if ($hadSave) { Copy-Item $backup $save -Force }
    elseif (Test-Path $save) { Remove-Item $save -Force }
    if (Test-Path $backup) { Remove-Item $backup -Force }
}
