param([switch]$Verify)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'Saved\Verification'),(Join-Path $projectRoot 'Saved\Cache') | Out-Null
$editor = 'D:\Games\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = Join-Path $projectRoot 'Luna_v1.uproject'
$existing=@(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | Where-Object {$_.CommandLine -and $_.CommandLine.Replace('/','\').Contains($project) -and $_.CommandLine -match ' -game '})
if($existing.Count -gt 0){
    if($Verify){throw 'Close the existing MVP game before starting verification.'}
    $shell=New-Object -ComObject WScript.Shell
    $null=$shell.AppActivate($existing[0].ProcessId)
    Write-Output 'The MVP game is already open.'
    return
}
$binary = Join-Path $projectRoot 'Binaries\Win64\UnrealEditor-LunaMVP.dll'
if (-not (Test-Path -LiteralPath $binary)) { throw 'Build the UE MVP first using Scripts\Build-Luna.ps1.' }
$args = @('"'+$project+'"','/Game/LunaArt/Maps/L_WanderingLuna','-game','-windowed','-ResX=1600','-ResY=900','-NoSplash','-ddc=InstalledNoZenLocalFallback',
    '"-LocalDataCachePath='+(Join-Path $projectRoot 'Saved\Cache')+'"',
    '-DisablePlugins=ModelContextProtocol,EditorToolset,ToolsetRegistry,PythonScriptPlugin,Terminal',
    '"-abslog='+(Join-Path $projectRoot 'Saved\Verification\runtime.log')+'"')
if ($Verify) { $args += '-LunaVerify' }
$process = Start-Process -FilePath $editor -ArgumentList $args -WorkingDirectory $projectRoot -WindowStyle Normal -PassThru
@{pid=$process.Id;verify=[bool]$Verify;project=$project;started_at=(Get-Date).ToString('o')} | ConvertTo-Json | Set-Content (Join-Path $projectRoot 'Saved\Verification\game-process.json') -Encoding utf8
Write-Output ('UE TiltPlay MVP started. Process '+$process.Id)
