# Restart Control API only (Windows PAPER). Leaves Dashboard / Market Core alone.
param(
    [string]$RepoRoot = '',
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$Root = Get-VsRoot -Hint $RepoRoot
Set-Location $Root
Assert-VsRepoRoot -Root $Root

if (Test-Path (Join-Path $Root '.env.paper')) { Import-DotEnvFile -Path (Join-Path $Root '.env.paper') }
elseif (Test-Path (Join-Path $Root '.env')) { Import-DotEnvFile -Path (Join-Path $Root '.env') }
Enforce-PaperFailClosed

$logs = Join-Path $Root 'logs'
if (-not (Test-Path -LiteralPath $logs)) { New-Item -ItemType Directory -Path $logs | Out-Null }

$apiBase = if ($env:CONTROL_API_URL) { $env:CONTROL_API_URL.TrimEnd('/') } else { 'http://127.0.0.1:3000' }
$apiPort = 3000
if ($env:CONTROL_API_PORT -match '^\d+$') { $apiPort = [int]$env:CONTROL_API_PORT }

Write-Step "Stopping anything on port $apiPort"
if (-not $DryRun) {
    try {
        $conns = Get-NetTCPConnection -LocalPort $apiPort -State Listen -ErrorAction SilentlyContinue
        foreach ($c in @($conns)) {
            if ($c.OwningProcess) {
                Write-Warn "Killing PID $($c.OwningProcess) on :$apiPort"
                Stop-Process -Id $c.OwningProcess -Force -ErrorAction SilentlyContinue
            }
        }
    } catch {
        Write-Warn "Port scan skipped: $($_.Exception.Message)"
    }
    Start-Sleep -Seconds 1
}

# Ensure docker deps if possible
try { Start-DockerDeps -Root $Root -DryRun:$DryRun } catch { Write-Warn $_.Exception.Message }

$distJs = Join-Path $Root 'apps\control-api\dist\index.js'
$envPaper = Join-Path $Root '.env.paper'
$nodeExe = Get-SystemNodeExe
if (-not (Test-Path -LiteralPath $distJs)) {
    throw 'apps\control-api\dist\index.js missing. Run Install.bat first.'
}

$migDist = Join-Path $Root 'apps\control-api\dist\db\migrations'
$migSrc = Join-Path $Root 'apps\control-api\src\db\migrations'
if (-not $DryRun -and (Test-Path -LiteralPath $migSrc)) {
    if (-not (Test-Path -LiteralPath $migDist) -or -not (Get-ChildItem -LiteralPath $migDist -Filter '*.sql' -ErrorAction SilentlyContinue)) {
        New-Item -ItemType Directory -Force -Path $migDist | Out-Null
        Copy-Item -Path (Join-Path $migSrc '*') -Destination $migDist -Force
    }
}

$apiExtra = @{
    OPERATING_MODE       = 'PAPER'
    LIVE_TRADING_ENABLED = 'false'
    CONTROL_API_HOST     = '0.0.0.0'
    CONTROL_API_PORT     = "$apiPort"
    DB_HOST              = $(if ($env:DB_HOST -and $env:DB_HOST -ne 'localhost') { $env:DB_HOST } else { '127.0.0.1' })
    REDIS_HOST           = $(if ($env:REDIS_HOST -and $env:REDIS_HOST -ne 'localhost') { $env:REDIS_HOST } else { '127.0.0.1' })
}
if (Test-Path -LiteralPath $envPaper) {
    $apiExtra['DOTENV_CONFIG_PATH'] = $envPaper
}

# Reuse the same visible CMD launcher pattern as start-v2.ps1
function Start-ApiWindow {
    param([string]$NodeExe, [string]$EntryJs, [hashtable]$ExtraEnv)
    $logPath = Join-Path $logs 'control-api.paper.log'
    $passKeys = @(
        'OPERATING_MODE', 'LIVE_TRADING_ENABLED',
        'DB_HOST', 'DB_PORT', 'DB_NAME', 'DB_USER', 'DB_PASSWORD',
        'REDIS_URL', 'REDIS_HOST', 'REDIS_PORT',
        'CONTROL_API_HOST', 'CONTROL_API_PORT', 'CONTROL_API_URL',
        'DOTENV_CONFIG_PATH',
        'API_ADMIN_TOKEN', 'ALLOW_INSECURE_ADMIN',
        'CORS_ORIGIN', 'CLIENT_CORS_ORIGIN', 'TRUST_PROXY',
        'MASTER_ENCRYPTION_KEY', 'JWT_SECRET', 'PIPELINE_TOKEN'
    )
    foreach ($k in $ExtraEnv.Keys) {
        Set-Item -Path "Env:$k" -Value ([string]$ExtraEnv[$k])
        [Environment]::SetEnvironmentVariable($k, [string]$ExtraEnv[$k], 'Process')
    }
    $envBlock = @('set OPERATING_MODE=PAPER', 'set LIVE_TRADING_ENABLED=false', 'set npm_config_prefix=', 'set PREFIX=')
    foreach ($k in $passKeys) {
        $v = [Environment]::GetEnvironmentVariable($k, 'Process')
        if ($null -ne $v -and "$v" -ne '') { $envBlock += ('set {0}={1}' -f $k, $v) }
    }
    foreach ($k in $ExtraEnv.Keys) { $envBlock += ('set {0}={1}' -f $k, $ExtraEnv[$k]) }

    $exePs = $NodeExe.Replace("'", "''")
    $logPs = $logPath.Replace("'", "''")
    $args = '"' + $EntryJs + '"'
    $psScript = @"
`$ErrorActionPreference = 'Continue'
Write-Host "Running: $NodeExe $args"
& '$exePs' $args 2>&1 | Tee-Object -FilePath '$logPs' -Append
exit `$LASTEXITCODE
"@
    $psFile = Join-Path $env:TEMP 'vs-v2-run-VS-ControlAPI-restart.ps1'
    Set-Content -LiteralPath $psFile -Value $psScript -Encoding ASCII
    $runLine = 'powershell -NoProfile -ExecutionPolicy Bypass -File "' + $psFile + '"'
    $cmd = @"
@echo off
title VS-ControlAPI
color 0A
cd /d "$Root"
$($envBlock -join "`r`n")
echo ============================================================
echo   VS-ControlAPI (restart)
echo   Log: $logPath
echo ============================================================
echo [%date% %time%] restart Control API>> "$logPath"
$runLine
set "RC=%ERRORLEVEL%"
echo [%date% %time%] exited Control API code=%RC%>> "$logPath"
echo.
echo [VS-ControlAPI] exited with code %RC%
pause
"@
    $launcher = Join-Path $env:TEMP 'vs-v2-VS-ControlAPI-restart.cmd'
    Set-Content -LiteralPath $launcher -Value $cmd -Encoding ASCII
    if ($DryRun) {
        Write-Host "[dry-run] start Control API $NodeExe $args"
        return
    }
    Start-Process -FilePath 'cmd.exe' -ArgumentList @('/k', "`"$launcher`"") -WorkingDirectory $Root -WindowStyle Normal | Out-Null
}

Write-Step 'Starting Control API window'
Write-Ok "node=$nodeExe entry=$distJs db_host=$($apiExtra.DB_HOST)"
Start-ApiWindow -NodeExe $nodeExe -EntryJs $distJs -ExtraEnv $apiExtra

if (-not $DryRun) {
    Write-Step "Waiting for $apiBase/health"
    if (-not (Wait-HttpOk -Url "$apiBase/health" -Attempts 90 -DelayMs 1000 -Label 'Control API /health')) {
        Write-LogTail -Path (Join-Path $logs 'control-api.paper.log') -Lines 80
        throw "Control API still unhealthy at $apiBase/health - check VS-ControlAPI window / log (often DB password or Docker not running)."
    }
    Write-Ok "Control API healthy at $apiBase/health"
}

exit 0
