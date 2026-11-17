param([ValidateSet('Editor','Game')][string]$Target = 'Editor')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'Saved\Verification'),(Join-Path $projectRoot 'Saved\Cache') | Out-Null
$dotnet = 'D:\Games\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe'
$ubt = 'D:\Games\Epic Games\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'
$name = if ($Target -eq 'Editor') { 'LunaMVPEditor' } else { 'LunaMVP' }
& $dotnet $ubt $name Win64 Development ('-Project=' + (Join-Path $projectRoot 'Luna_v1.uproject')) -WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles
exit $LASTEXITCODE
