# Build + serve public Client Control Panel (Windows).
# Static dist-client + client-gateway on :5174 -> Control API :3000
# Does NOT switch OPERATING_MODE / does NOT place orders.
param(
    [string]$RepoRoot = '',
    [switch]$DryRun,
    [switch]$BuildOnly
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$Root = Get-VsRoot -Hint $RepoRoot
Set-Location $Root
Assert-VsRepoRoot -Root $Root

if (Test-Path (Join-Path $Root '.env.live')) { Import-DotEnvFile -Path (Join-Path $Root '.env.live') }
elseif (Test-Path (Join-Path $Root '.env.paper')) { Import-DotEnvFile -Path (Join-Path $Root '.env.paper') }
elseif (Test-Path (Join-Path $Root '.env')) { Import-DotEnvFile -Path (Join-Path $Root '.env') }

if (-not $env:CLIENT_PUBLIC_PORT) { $env:CLIENT_PUBLIC_PORT = '5174' }
if (-not $env:CLIENT_COOKIE_SECURE) { $env:CLIENT_COOKIE_SECURE = 'true' }
if (-not $env:TRUST_PROXY) { $env:TRUST_PROXY = 'true' }
if (-not $env:CLIENT_CORS_ORIGIN -or "$($env:CLIENT_CORS_ORIGIN)".Trim() -eq '') {
    $env:CLIENT_CORS_ORIGIN = 'http://127.0.0.1:5174,http://localhost:5174'
    Write-Warn 'CLIENT_CORS_ORIGIN unset - local :5174 only. Set public HTTPS origin in .env.live for remote clients.'
}

$logs = Join-Path $Root 'logs'
if (-not (Test-Path -LiteralPath $logs)) {
    if (-not $DryRun) { New-Item -ItemType Directory -Path $logs | Out-Null }
}

Write-Step 'Building public Client Web'
$clientDist = Ensure-ClientWebDist -Root $Root -DryRun:$DryRun
$env:CLIENT_DIST = $clientDist
$env:CLIENT_PANEL_DIST = $clientDist

$clientUrl = Resolve-ClientWebUrl
Write-Host "  Local gateway:  $clientUrl"
Write-Host '  Put Cloudflare / nginx TLS in front for public HTTPS.'
Write-Host "  CLIENT_COOKIE_SECURE=$($env:CLIENT_COOKIE_SECURE) TRUST_PROXY=$($env:TRUST_PROXY)"
Write-Host "  CLIENT_CORS_ORIGIN=$($env:CLIENT_CORS_ORIGIN)"

if ($BuildOnly) {
    Write-Ok 'Build only (-BuildOnly) - gateway not started'
    exit 0
}

$nodeExe = Get-SystemNodeExe
$gatewayJs = Join-Path $Root 'apps\dashboard\client-gateway.mjs'
if (-not (Test-Path -LiteralPath $gatewayJs)) {
    throw "Missing $gatewayJs"
}

$apiBase = if ($env:CONTROL_API_URL) { $env:CONTROL_API_URL.TrimEnd('/') } else { 'http://127.0.0.1:3000' }
if (-not $DryRun) {
    if (Wait-HttpOk -Url "$apiBase/health" -Attempts 3 -DelayMs 300 -Quiet) {
        Write-Ok "Control API healthy at $apiBase/health"
    } else {
        Write-Warn "Control API not reachable at $apiBase/health - start LIVE.bat / Restart-ControlAPI.bat before client login"
    }
}

$logPath = Join-Path $logs 'client-web.deploy.log'
$envBlock = @(
    ('set CLIENT_PUBLIC_PORT={0}' -f $env:CLIENT_PUBLIC_PORT),
    ('set CLIENT_DIST={0}' -f $clientDist),
    ('set CLIENT_PANEL_DIST={0}' -f $clientDist),
    'set CONTROL_API_HOST=127.0.0.1',
    'set CONTROL_API_PORT=3000',
    ('set CLIENT_COOKIE_SECURE={0}' -f $env:CLIENT_COOKIE_SECURE),
    ('set TRUST_PROXY={0}' -f $env:TRUST_PROXY),
    ('set CLIENT_CORS_ORIGIN={0}' -f $env:CLIENT_CORS_ORIGIN)
)
$cmd = @"
@echo off
title VS-ClientWeb
color 0B
cd /d "$Root\apps\dashboard"
$($envBlock -join "`r`n")
echo ============================================================
echo   VS-ClientWeb  public client panel
echo   http://127.0.0.1:$($env:CLIENT_PUBLIC_PORT)/
echo   Log: $logPath
echo ============================================================
echo [%date% %time%] starting client-gateway>> "$logPath"
"$nodeExe" "$gatewayJs" 1>> "$logPath" 2>&1
set "RC=%ERRORLEVEL%"
echo [%date% %time%] exited code=%RC%>> "$logPath"
echo [VS-ClientWeb] exited %RC%
pause
"@

if ($DryRun) {
    Write-Host '[dry-run] would start VS-ClientWeb gateway'
    exit 0
}

$launcher = Join-Path $env:TEMP 'vs-v2-VS-ClientWeb.cmd'
Set-Content -LiteralPath $launcher -Value $cmd -Encoding ASCII
Start-Process -FilePath 'cmd.exe' -ArgumentList @('/k', "`"$launcher`"") -WorkingDirectory $Root -WindowStyle Normal | Out-Null
Write-Ok "VS-ClientWeb CMD opened - $clientUrl"
if (-not (Wait-HttpOk -Url $clientUrl -Attempts 30 -DelayMs 500 -Label 'Client Web')) {
    Write-LogTail -Path $logPath -Lines 40
    throw "Client Web gateway did not respond at $clientUrl"
}
Write-Ok "Client Web live at $clientUrl"
exit 0
