param([Parameter(Mandatory=$true)][ValidateSet('baseline','modified','rollback')][string]$Phase)
$ErrorActionPreference='Stop'
$run='E:/UNREAL/ue projects/ThirdPerson/Saved/ForestUIBuild/20260906_v1'
$project=if($Phase -eq 'modified'){"$run/stage/ThirdPerson"}else{"$run/rollback_test/ThirdPerson"}
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw | ConvertFrom-Json
foreach($entry in $manifest.entries){
  $p=Join-Path $project $entry.path
  $expected=if($Phase -eq 'modified'){$entry.modified_sha256}else{$entry.original_sha256}
  if($expected){
    if((Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash -ne $expected){throw ('Hash mismatch: '+$entry.path)}
  }elseif(Test-Path -LiteralPath $p){throw ('Added file still present: '+$entry.path)}
}
$engine='D:/Unreal5.8/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$lines=& $engine "$project/ThirdPerson.uproject" -run=pythonscript "-script=$run/test_ui.py" '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities' "-ForestPhase=$Phase" -unattended -NullRHI -nosound -nop4 "-abslog=$run/${Phase}_engine.log" -stdout -FullStdOutLogOutput 2>&1
$code=$LASTEXITCODE
$lines | Set-Content -LiteralPath "$run/${Phase}_console.log" -Encoding UTF8
$summary=[regex]::Match(($lines -join "`n"),'(?m)LogPython: ('+$Phase.ToUpper()+' core=PASS[^\r\n]*)').Groups[1].Value
if($code -ne 0 -or -not $summary){throw "UE test failed; exit=$code; see $run/${Phase}_console.log"}
$result=Get-Content -LiteralPath "$run/${Phase}_test.json" -Raw | ConvertFrom-Json
if(-not $result.pass){throw 'Runtime contract failed'}
if($result.hp_percent_matches_50 -ne ($Phase -eq 'modified')){throw 'Unexpected baseline/modified behavior'}
Write-Output $summary
if($Phase -eq 'rollback'){Write-Output 'ROLLBACK restored_hashes=PASS'}
exit 0
