# Restart Control API only (Windows PAPER). Leaves Dashboard / Market Core alone.
# ALWAYS starts via absolute node.exe + apps\control-api\dist\index.js - NEVER npm.
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

try { Start-DockerDeps -Root $Root -DryRun:$DryRun } catch { Write-Warn $_.Exception.Message }

$distJs = Join-Path $Root 'apps\control-api\dist\index.js'
$envPaper = Join-Path $Root '.env.paper'
$nodeExe = Get-SystemNodeExe
if (-not (Test-Path -LiteralPath $distJs)) {
    throw 'apps\control-api\dist\index.js missing. Run Install.bat first.'
}
if ($nodeExe -match '(?i)npm') {
    throw "Refusing npm as node.exe: $nodeExe"
}

$migDist = Join-Path $Root 'apps\control-api\dist\db\migrations'
$migSrc = Join-Path $Root 'apps\control-api\src\db\migrations'
if (-not $DryRun -and (Test-Path -LiteralPath $migSrc)) {
    if (-not (Test-Path -LiteralPath $migDist) -or -not (Get-ChildItem -LiteralPath $migDist -Filter '*.sql' -ErrorAction SilentlyContinue)) {
        New-Item -ItemType Directory -Force -Path $migDist | Out-Null
        Copy-Item -Path (Join-Path $migSrc '*') -Destination $migDist -Force
    }
}

$dbHost = '127.0.0.1'
if ($env:DB_HOST -and $env:DB_HOST -ne 'localhost') { $dbHost = $env:DB_HOST }
$redisHost = '127.0.0.1'
if ($env:REDIS_HOST -and $env:REDIS_HOST -ne 'localhost') { $redisHost = $env:REDIS_HOST }

$passKeys = @(
    'DB_PORT', 'DB_NAME', 'DB_USER', 'DB_PASSWORD',
    'REDIS_URL', 'REDIS_PORT',
    'CONTROL_API_URL',
    'API_ADMIN_TOKEN', 'ALLOW_INSECURE_ADMIN',
    'CORS_ORIGIN', 'CLIENT_CORS_ORIGIN', 'TRUST_PROXY',
    'MASTER_ENCRYPTION_KEY', 'JWT_SECRET', 'PIPELINE_TOKEN'
)

$envBlock = @(
    'set OPERATING_MODE=PAPER',
    'set LIVE_TRADING_ENABLED=false',
    'set CONTROL_API_HOST=0.0.0.0',
    ('set CONTROL_API_PORT={0}' -f $apiPort),
    ('set DB_HOST={0}' -f $dbHost),
    ('set REDIS_HOST={0}' -f $redisHost),
    'set npm_config_prefix=',
    'set PREFIX=',
    'set VITE_API_URL='
)
if (Test-Path -LiteralPath $envPaper) {
    $envBlock += ('set DOTENV_CONFIG_PATH={0}' -f $envPaper)
}
foreach ($k in $passKeys) {
    $v = [Environment]::GetEnvironmentVariable($k, 'Process')
    if ($null -ne $v -and "$v" -ne '') { $envBlock += ('set {0}={1}' -f $k, $v) }
}

$logPath = Join-Path $logs 'control-api.paper.log'
# Pure CMD - no PowerShell wrapper, no npm.cmd, no npm.ps1, no $args.
$cmd = @"
@echo off
title VS-ControlAPI
color 0A
cd /d "$Root"
$($envBlock -join "`r`n")
echo ============================================================
echo   VS-ControlAPI (node.exe only - never npm)
echo   node: $nodeExe
echo   entry: $distJs
echo   Log: $logPath
echo ============================================================
echo [%date% %time%] restart Control API>> "$logPath"
echo [%date% %time%] exe=$nodeExe>> "$logPath"
echo [%date% %time%] entry=$distJs>> "$logPath"
echo Starting (node only):
echo   "$nodeExe" "$distJs"
"$nodeExe" "$distJs" 1>> "$logPath" 2>&1
set "RC=%ERRORLEVEL%"
echo [%date% %time%] exited Control API code=%RC%>> "$logPath"
echo.
echo [VS-ControlAPI] exited with code %RC%
echo If you see npm-cli.js / npm-prefix.js errors, this launcher is OLD - pull main and re-run.
echo Log: $logPath
pause
"@

$launcher = Join-Path $env:TEMP 'vs-v2-VS-ControlAPI-node-only.cmd'
Set-Content -LiteralPath $launcher -Value $cmd -Encoding ASCII

Write-Step 'Starting Control API window (node.exe only)'
Write-Ok "node=$nodeExe"
Write-Ok "entry=$distJs"
Write-Ok "db_host=$dbHost"
if ($DryRun) {
    Write-Host "[dry-run] would start $launcher"
    exit 0
}
Start-Process -FilePath 'cmd.exe' -ArgumentList @('/k', "`"$launcher`"") -WorkingDirectory $Root -WindowStyle Normal | Out-Null

Write-Step "Waiting for $apiBase/health"
if (-not (Wait-HttpOk -Url "$apiBase/health" -Attempts 90 -DelayMs 1000 -Label 'Control API /health')) {
    Write-LogTail -Path $logPath -Lines 80
    throw "Control API still unhealthy at $apiBase/health - check VS-ControlAPI window / log (DB password, Docker, or missing dist)."
}
Write-Ok "Control API healthy at $apiBase/health"
exit 0
