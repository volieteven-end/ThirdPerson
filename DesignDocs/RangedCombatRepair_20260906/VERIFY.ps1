param([ValidateSet('BASELINE','MODIFIED','ROLLBACK')][string]$Case)
& 'C:/Users/Windows11/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' (Join-Path $PSScriptRoot 'VERIFY.py') $Case
exit $LASTEXITCODE