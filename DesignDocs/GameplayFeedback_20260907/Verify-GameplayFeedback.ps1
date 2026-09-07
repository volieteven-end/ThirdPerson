param(
    [string]$EngineRoot = 'D:\Unreal5.8\UE_5.8',
    [ValidateSet('All', 'NullRHI', 'RHI')][string]$Render = 'All',
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$Project = Join-Path $ProjectRoot 'ThirdPerson.uproject'
$Build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$Editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
foreach ($File in @($Project, $Build, $Editor)) {
    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) { throw "Missing input: $File" }
}
$Run = Join-Path $ProjectRoot ('Saved\Automation\GameplayFeedbackVerification_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Path $Run -Force | Out-Null
$Records = [Collections.Generic.List[object]]::new()
try {
    if (-not $SkipBuild) {
        foreach ($Target in @('ThirdPersonEditor', 'ThirdPerson')) {
            $Arguments = @($Target, 'Win64', 'Development', "-Project=$Project", '-WaitMutex', '-NoHotReloadFromIDE', '-NoUBTMakefiles')
            $Log = Join-Path $Run ($Target + '_Build.txt')
            & $Build @Arguments *> $Log
            $Code = $LASTEXITCODE
            $Records.Add([ordered]@{ phase = $Target; executable = $Build; arguments = $Arguments; exit = $Code; log = $Log })
            if ($Code -ne 0) { throw "$Target build failed ($Code); $Log" }
            Write-Output "BUILD $Target EXIT=0"
        }
    }
    $Modes = if ($Render -eq 'All') { @('NullRHI', 'RHI') } else { @($Render) }
    foreach ($Mode in $Modes) {
        $Output = Join-Path $Run $Mode
        $Log = Join-Path $Run ($Mode + '_Engine.txt')
        $Arguments = @($Project, '/Engine/Maps/Entry', '-ExecCmds=Automation RunTests ThirdPerson',
            '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$($Output.Replace('\','/'))",
            '-nosound', '-unattended', '-nop4', '-stdout', "-abslog=$($Log.Replace('\','/'))")
        if ($Mode -eq 'NullRHI') { $Arguments += '-NullRHI' }
        else { $Arguments += @('-RenderOffscreen', '-FeedbackVisualAudit', '-ResX=1280', '-ResY=720') }
        & $Editor @Arguments *> (Join-Path $Run ($Mode + '_Console.txt'))
        $Code = $LASTEXITCODE
        # Unreal may exit zero even when an Automation assertion fails. Read the fresh report.
        $Index = Join-Path $Output 'index.json'
        if (-not (Test-Path -LiteralPath $Index)) { throw "$Mode did not export $Index" }
        $Report = Get-Content -Raw -LiteralPath $Index | ConvertFrom-Json
        $Passed = [int]$Report.succeeded + [int]$Report.succeededWithWarnings
        $Bad = @($Report.tests | Where-Object { $_.state -ne 'Success' })
        $Records.Add([ordered]@{ phase = $Mode; executable = $Editor; arguments = $Arguments;
            exit = $Code; passed = $Passed; failed = $Report.failed; nonSuccess = $Bad.Count; report = $Index; log = $Log })
        if ($Code -ne 0 -or $Report.failed -ne 0 -or $Bad.Count -ne 0 -or $Passed -lt 34) {
            throw "$Mode validation failed: exit=$Code passed=$Passed failed=$($Report.failed) nonSuccess=$($Bad.Count); $Index"
        }
        foreach ($Trace in @('movement_trace.csv','boss_restart_trace.csv','boss_raised_restart_trace.csv')) {
            $Source = Join-Path $ProjectRoot ('Saved\GameplayFeedback\' + $Trace)
            if (Test-Path -LiteralPath $Source) { Copy-Item -LiteralPath $Source -Destination (Join-Path $Output $Trace) }
        }
        Write-Output "TEST $Mode PASS=$Passed FAIL=0 EXIT=0"
    }
    Write-Output "VERIFIED $Run"
} finally {
    ConvertTo-Json -InputObject @($Records.ToArray()) -Depth 10 | Set-Content -LiteralPath (Join-Path $Run 'commands-and-results.json') -Encoding utf8
}
