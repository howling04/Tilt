$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'Saved\Verification'),(Join-Path $projectRoot 'Saved\Cache') | Out-Null
$saved = Join-Path $projectRoot 'Saved\Verification'
& 'D:\Games\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' (Join-Path $projectRoot 'Luna_v1.uproject') '-ExecCmds=Automation RunTests LunaMVP' '-TestExit=Automation Test Queue Empty' ('-ReportExportPath='+(Join-Path $saved 'Automation')) -unattended -NullRHI -nosound -NoSplash -ddc=InstalledNoZenLocalFallback '-DisablePlugins=ModelContextProtocol,EditorToolset,ToolsetRegistry,PythonScriptPlugin,Terminal' ('-LocalDataCachePath='+(Join-Path $projectRoot 'Saved\Cache')) ('-abslog='+(Join-Path $saved 'automation.log')) *> (Join-Path $saved 'automation-console.log')
$code=$LASTEXITCODE
Select-String -Path (Join-Path $saved 'automation.log') -Pattern 'Test Completed','Test Succeeded','Test Failed','LunaMVP','TEST COMPLETE' | Select-Object -Last 16 | ForEach-Object Line
exit $code

