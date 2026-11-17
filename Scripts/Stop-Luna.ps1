$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'Saved\Verification'),(Join-Path $projectRoot 'Saved\Cache') | Out-Null
$record=Join-Path $projectRoot 'Saved\Verification\game-process.json'
if(-not (Test-Path -LiteralPath $record)){return}
$info=Get-Content -LiteralPath $record -Raw | ConvertFrom-Json
$process=Get-CimInstance Win32_Process -Filter ('ProcessId='+[int]$info.pid)
if(-not $process){return}
$project=Join-Path $projectRoot 'Luna_v1.uproject'
if($process.Name -ne 'UnrealEditor.exe' -or -not $process.CommandLine.Contains($project) -or $process.CommandLine -notmatch ' -game '){throw 'Recorded process is not this MVP game. Left running.'}
$owned=Get-Process -Id $process.ProcessId
if($owned.MainWindowHandle -ne 0){$null=$owned.CloseMainWindow();$null=$owned.WaitForExit(5000)}
if(-not $owned.HasExited){Stop-Process -Id $owned.Id}
Write-Output 'Stopped only the MVP game window.'
