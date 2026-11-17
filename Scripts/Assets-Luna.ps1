param([switch]$Audit,[switch]$MaterialsOnly)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'Saved\Verification'),(Join-Path $projectRoot 'Saved\Cache') | Out-Null
$editor='D:\Games\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$params=@((Join-Path $projectRoot 'Luna_v1.uproject'),'-run=pythonscript',('-script='+(Join-Path $projectRoot 'SourceArt\import.py')),'-unattended','-nullrhi','-NoSplash','-DisablePlugins=ModelContextProtocol,EditorToolset,ToolsetRegistry,Terminal','-ddc=InstalledNoZenLocalFallback',('-LocalDataCachePath='+(Join-Path $projectRoot 'Saved\Cache')),('-abslog='+(Join-Path $projectRoot 'Saved\Verification\assets.log')))
if($Audit){$params+='-LunaAudit'}
if($MaterialsOnly){$params+='-LunaMaterialsOnly'}
& $editor @params
exit $LASTEXITCODE
