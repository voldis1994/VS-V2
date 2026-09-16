# VS-V2 daily Windows LIVE launch (LIVE.bat)
# ONE double-click (after typing LIVE) starts everything:
#   1) postgres + redis (docker)
#   2) Control API  -> visible CMD window (OPERATING_MODE=LIVE)
#   3) Market Core  -> visible CMD window (--mode LIVE, Capital execution bound)
#   4) Dashboard    -> visible CMD window (admin Control Panel :5173)
#   5) Client Web   -> visible CMD window (public gateway :5174)
#   6) browser -> Control Panel
# Does NOT reinstall. Real Capital open/close orders allowed when gates pass.
param(
    [string]$RepoRoot = '',
    [switch]$DryRun,
    [switch]$NoBrowser,
    [switch]$SkipMarketCore,
    [switch]$ConfirmLive
)

$ErrorActionPreference = 'Stop'
$launchLog = $null

try {
. (Join-Path $PSScriptRoot 'common.ps1')

if (-not $ConfirmLive) {
    throw 'Refusing LIVE start without -ConfirmLive (run LIVE.bat and type LIVE).'
}

$Root = Get-VsRoot -Hint $RepoRoot
Set-Location $Root
Assert-VsRepoRoot -Root $Root
$env:VS_V2_ROOT = $Root
[Environment]::SetEnvironmentVariable('VS_V2_ROOT', $Root, 'Process')

$logs = Join-Path $Root 'logs'
if (-not (Test-Path -LiteralPath $logs)) {
    New-Item -ItemType Directory -Path $logs | Out-Null
}
$launchLog = Join-Path $logs 'live-launch.log'
try {
    Start-Transcript -Path $launchLog -Force | Out-Null
} catch {
    Write-Warn "Could not start transcript: $($_.Exception.Message)"
}

Write-Host ''
Write-Host '============================================================' -ForegroundColor Red
Write-Host '  VS-V2 LIVE.bat - daily LIVE launch (Capital orders ON)' -ForegroundColor Red
Write-Host '============================================================' -ForegroundColor Red
Write-Host "  Root: $Root"
Write-Host "  Log:  $launchLog"
Write-Host '  Opens CMD windows: Control API + Market Core + Dashboard + Client Web + Cloudflare'
Write-Host '  Mode: LIVE | Live trading: ON | Client Web :5174 | Broker open/close: armed'
Write-Host ''

$marker = Join-Path $Root '.vs-v2-installed'
if (-not (Test-Path -LiteralPath $marker)) {
    Write-Warn 'Install marker .vs-v2-installed missing - run Install.bat once first'
    if (-not $DryRun -and -not (Test-Path (Join-Path $Root 'node_modules'))) {
        throw 'node_modules missing. Run Install.bat before LIVE.bat.'
    }
}

Write-Step 'Loading env (.env.live / .env.paper / .env)'
if (Test-Path (Join-Path $Root '.env.live')) {
    Import-DotEnvFile -Path (Join-Path $Root '.env.live')
    Write-Ok 'loaded .env.live'
} elseif (Test-Path (Join-Path $Root '.env.paper')) {
    Import-DotEnvFile -Path (Join-Path $Root '.env.paper')
    Write-Ok 'loaded .env.paper'
} elseif (Test-Path (Join-Path $Root '.env')) {
    Import-DotEnvFile -Path (Join-Path $Root '.env')
    Write-Ok 'loaded .env'
} else {
    Write-Warn 'No .env.live / .env.paper / .env found - using process env only'
}

Enforce-LiveArmed
Assert-LiveArmed
$hasCapital = Test-LiveCapitalCredentials
if (-not $hasCapital -and -not $SkipMarketCore) {
    Write-Warn 'Will still start Control API + Dashboard + Client Web; Market Core may exit on Capital auth.'
}

# Public client web (:5174) - cookies/CORS for gateway + optional HTTPS tunnel.
if (-not $env:CLIENT_PUBLIC_PORT) { $env:CLIENT_PUBLIC_PORT = '5174' }
if (-not $env:CLIENT_COOKIE_SECURE) { $env:CLIENT_COOKIE_SECURE = 'true' }
if (-not $env:TRUST_PROXY) { $env:TRUST_PROXY = 'true' }
if (-not $env:CLIENT_CORS_ORIGIN -or "$($env:CLIENT_CORS_ORIGIN)".Trim() -eq '') {
    $env:CLIENT_CORS_ORIGIN = 'http://127.0.0.1:5174,http://localhost:5174,http://127.0.0.1:5173,http://localhost:5173'
    Write-Warn 'CLIENT_CORS_ORIGIN unset - defaulting to local :5173/:5174. For public HTTPS set your tunnel/domain in .env.live'
}
if (-not $env:CORS_ORIGIN -or "$($env:CORS_ORIGIN)".Trim() -eq '') {
    $env:CORS_ORIGIN = 'http://127.0.0.1:5173,http://localhost:5173'
}

if (-not $env:API_ADMIN_TOKEN -or $env:API_ADMIN_TOKEN -eq 'CHANGE_ME_ADMIN_TOKEN') {
    if ($env:ALLOW_INSECURE_ADMIN -ne 'true') {
        Write-Warn 'API_ADMIN_TOKEN is CHANGE_ME/empty - setting ALLOW_INSECURE_ADMIN=true for local LIVE start'
        $env:ALLOW_INSECURE_ADMIN = 'true'
    }
} else {
    Write-Ok 'API_ADMIN_TOKEN loaded for dashboard proxy'
}

Write-Ok 'Forced OPERATING_MODE=LIVE LIVE_TRADING_ENABLED=true'
Write-RuntimeModeMarker -Root $Root -Mode 'LIVE'

Write-Step 'Ensuring postgres + redis (Docker Desktop must be running)'
Write-Host '  If this hangs >60s: open Docker Desktop, wait until it is green, re-run LIVE.bat'
try {
    Start-DockerDeps -Root $Root -DryRun:$DryRun
} catch {
    Write-Warn $_.Exception.Message
    Write-Warn 'Continuing - if DB is already local, API may still work'
}

# Opens a visible CMD window that stays open (/k). LIVE services only.
function Start-LiveLoggedProcess {
    param(
        [string]$Title,
        [string]$FilePath,
        [string]$Arguments,
        [string]$WorkingDirectory,
        [string]$LogName,
        [hashtable]$ExtraEnv
    )
    if ($Arguments -match '(?i)--mode\s+PAPER') {
        throw "Safety abort: LIVE launcher refused PAPER market-core mode: $Arguments"
    }
    if ($Arguments -match '(?i)--mode\s+SHADOW') {
        throw "Safety abort: LIVE launcher refused SHADOW market-core mode: $Arguments"
    }
    if ($DryRun) {
        Write-Host "[dry-run] start $Title :: $FilePath $Arguments"
        return $null
    }

    $exe = $FilePath
    if ($FilePath -match '(?i)^npm(\.cmd)?$') {
        $resolvedNpm = Resolve-Tool -Name 'npm'
        if ($resolvedNpm) { $exe = $resolvedNpm }
        else { $exe = Join-Path ${env:ProgramFiles} 'nodejs\npm.cmd' }
    } elseif ($FilePath -match '(?i)^node(\.exe)?$') {
        $resolvedNode = Resolve-Tool -Name 'node'
        if ($resolvedNode) { $exe = $resolvedNode }
    }
    if (-not [System.IO.Path]::IsPathRooted($exe)) {
        $cmdProbe = Get-Command $exe -ErrorAction SilentlyContinue
        if ($cmdProbe -and $cmdProbe.Source) { $exe = $cmdProbe.Source }
    }

    foreach ($k in $ExtraEnv.Keys) {
        Set-Item -Path "Env:$k" -Value ([string]$ExtraEnv[$k])
        [Environment]::SetEnvironmentVariable($k, [string]$ExtraEnv[$k], 'Process')
    }
    $passKeys = @(
        'OPERATING_MODE', 'LIVE_TRADING_ENABLED', 'MARKET_CORE_BRIDGE',
        'DB_HOST', 'DB_PORT', 'DB_NAME', 'DB_USER', 'DB_PASSWORD',
        'REDIS_URL', 'REDIS_HOST', 'REDIS_PORT',
        'CONTROL_API_HOST', 'CONTROL_API_PORT', 'CONTROL_API_URL',
        'DOTENV_CONFIG_PATH',
        'API_ADMIN_TOKEN', 'ALLOW_INSECURE_ADMIN',
        'CORS_ORIGIN', 'CLIENT_CORS_ORIGIN', 'TRUST_PROXY',
        'CLIENT_COOKIE_SECURE', 'CLIENT_PUBLIC_PORT', 'CLIENT_DIST', 'CLIENT_PANEL_DIST', 'CLIENT_PUBLIC_URL',
        'CAPITAL_API_KEY', 'CAPITAL_API_PASSWORD', 'CAPITAL_IDENTIFIER', 'CAPITAL_EPIC', 'CAPITAL_BASE_URL',
        'MASTER_ENCRYPTION_KEY', 'JWT_SECRET', 'PIPELINE_TOKEN',
        'VS_V2_MODEL_PATH', 'VS_V2_MODEL_ID', 'VS_V2_MODEL_VERSION'
    )
    $envBlock = @(
        'set OPERATING_MODE=LIVE',
        'set LIVE_TRADING_ENABLED=true',
        'set npm_config_prefix=',
        'set PREFIX='
    )
    foreach ($k in $passKeys) {
        $v = [Environment]::GetEnvironmentVariable($k, 'Process')
        if ($null -ne $v -and "$v" -ne '') {
            $envBlock += ('set {0}={1}' -f $k, $v)
        }
    }
    foreach ($k in $ExtraEnv.Keys) {
        $envBlock += ('set {0}={1}' -f $k, $ExtraEnv[$k])
    }

    $logPath = Join-Path $logs $LogName
    # Pure CMD redirect (ASCII/ANSI) - NEVER Tee-Object (writes UTF-16 LE garble in Notepad/VS Code).
    $cmd = @"
@echo off
title $Title
color 0C
cd /d "$WorkingDirectory"
$($envBlock -join "`r`n")
echo ============================================================
echo   $Title
echo   LIVE - Capital broker orders ARMED
echo   Log: $logPath
echo   Close this window to stop this service.
echo ============================================================
echo [%date% %time%] starting $Title>> "$logPath"
echo [%date% %time%] exe=$exe args=$Arguments>> "$logPath"
echo Starting: $exe $Arguments
"$exe" $Arguments 1>> "$logPath" 2>&1
set "RC=%ERRORLEVEL%"
echo [%date% %time%] exited $Title code=%RC%>> "$logPath"
echo.
echo [$Title] exited with code %RC%
echo Log: $logPath
echo.
pause
"@
    $launcher = Join-Path $env:TEMP ("vs-v2-live-" + $Title + '.cmd')
    Set-Content -LiteralPath $launcher -Value $cmd -Encoding ASCII
    $p = Start-Process -FilePath 'cmd.exe' -ArgumentList @('/k', "`"$launcher`"") -WorkingDirectory $WorkingDirectory -PassThru -WindowStyle Normal
    Set-Content -LiteralPath (Join-Path $logs ($LogName + '.pid')) -Value $p.Id
    Write-Ok "$Title CMD opened pid=$($p.Id) log=$logPath"
    Write-Host "  exe: $exe $Arguments"
    return $p
}

Write-Step 'Starting all LIVE services (3 CMD windows)'

# --- 1) Control API via absolute node.exe (never npm) ---
$apiLog = Join-Path $logs 'control-api.live.log'
$envLive = Join-Path $Root '.env.live'
$envPaper = Join-Path $Root '.env.paper'
$nodeExe = Get-SystemNodeExe
# Rebuild when src newer than dist (git pull otherwise leaves FEED/NEWS as Fastify Not Found).
$distJs = Ensure-ControlApiDist -Root $Root -DryRun:$DryRun

$dotenvPath = $null
if (Test-Path -LiteralPath $envLive) { $dotenvPath = $envLive }
elseif (Test-Path -LiteralPath $envPaper) { $dotenvPath = $envPaper }

$apiExtra = @{
    OPERATING_MODE         = 'LIVE'
    LIVE_TRADING_ENABLED   = 'true'
    MARKET_CORE_BRIDGE     = $(if ($env:MARKET_CORE_BRIDGE) { $env:MARKET_CORE_BRIDGE } else { 'true' })
    CONTROL_API_HOST       = '0.0.0.0'
    CONTROL_API_PORT       = '3000'
}
if (-not $env:DB_HOST -or $env:DB_HOST -eq 'localhost') {
    $apiExtra['DB_HOST'] = '127.0.0.1'
    $env:DB_HOST = '127.0.0.1'
}
if (-not $env:REDIS_HOST -or $env:REDIS_HOST -eq 'localhost') {
    $apiExtra['REDIS_HOST'] = '127.0.0.1'
    $env:REDIS_HOST = '127.0.0.1'
}
if ($dotenvPath) {
    $apiExtra['DOTENV_CONFIG_PATH'] = $dotenvPath
}

$migDist = Join-Path $Root 'apps\control-api\dist\db\migrations'
$migSrc = Join-Path $Root 'apps\control-api\src\db\migrations'
if (-not $DryRun -and (Test-Path -LiteralPath $migSrc)) {
    if (-not (Test-Path -LiteralPath $migDist) -or -not (Get-ChildItem -LiteralPath $migDist -Filter '*.sql' -ErrorAction SilentlyContinue)) {
        Write-Warn 'control-api dist migrations missing - copying src\\db\\migrations -> dist\\db\\migrations'
        New-Item -ItemType Directory -Force -Path $migDist | Out-Null
        Copy-Item -Path (Join-Path $migSrc '*') -Destination $migDist -Force
    }
    $sqlCount = @(Get-ChildItem -LiteralPath $migDist -Filter '*.sql' -ErrorAction SilentlyContinue).Count
    Write-Ok "control-api migrations ready ($sqlCount sql files in dist\\db\\migrations)"
}

Write-Ok "control-api via node.exe (not npm): $nodeExe"
Write-Host "  entry: $distJs"
if ($apiExtra.ContainsKey('DOTENV_CONFIG_PATH')) { Write-Host "  env:   DOTENV_CONFIG_PATH=$dotenvPath" }

$apiEnvLines = @(
    'set OPERATING_MODE=LIVE',
    'set LIVE_TRADING_ENABLED=true',
    'set CONTROL_API_HOST=0.0.0.0',
    'set CONTROL_API_PORT=3000',
    ('set VS_V2_ROOT={0}' -f $Root),
    'set npm_config_prefix=',
    'set PREFIX=',
    'set VITE_API_URL='
)
foreach ($k in @('DB_HOST','DB_PORT','DB_NAME','DB_USER','DB_PASSWORD','REDIS_HOST','REDIS_PORT','REDIS_URL','DOTENV_CONFIG_PATH','API_ADMIN_TOKEN','ALLOW_INSECURE_ADMIN','CORS_ORIGIN','CLIENT_CORS_ORIGIN','TRUST_PROXY','CLIENT_COOKIE_SECURE','CLIENT_PUBLIC_PORT','CLIENT_PUBLIC_URL','MASTER_ENCRYPTION_KEY','JWT_SECRET','PIPELINE_TOKEN','CONTROL_API_URL','MARKET_CORE_BRIDGE','CAPITAL_API_KEY','CAPITAL_API_PASSWORD','CAPITAL_IDENTIFIER','CAPITAL_EPIC','CAPITAL_BASE_URL','VS_V2_ROOT')) {
    if ($apiExtra.ContainsKey($k)) {
        $apiEnvLines += ('set {0}={1}' -f $k, $apiExtra[$k])
    } else {
        $v = [Environment]::GetEnvironmentVariable($k, 'Process')
        if ($null -ne $v -and "$v" -ne '') { $apiEnvLines += ('set {0}={1}' -f $k, $v) }
    }
}
$apiCmd = @"
@echo off
title VS-ControlAPI
color 0C
cd /d "$Root"
$($apiEnvLines -join "`r`n")
echo ============================================================
echo   VS-ControlAPI
echo   LIVE - node.exe ONLY (never npm / npm.ps1)
echo   OPERATING_MODE=LIVE LIVE_TRADING_ENABLED=true
echo   node: $nodeExe
echo   entry: $distJs
echo   Log: $apiLog
echo   Close this window to stop Control API.
echo ============================================================
echo [%date% %time%] starting VS-ControlAPI LIVE>> "$apiLog"
echo [%date% %time%] exe=$nodeExe>> "$apiLog"
echo [%date% %time%] entry=$distJs>> "$apiLog"
echo Starting:
echo   "$nodeExe" "$distJs"
"$nodeExe" "$distJs" 1>> "$apiLog" 2>&1
set "RC=%ERRORLEVEL%"
echo [%date% %time%] exited VS-ControlAPI code=%RC%>> "$apiLog"
echo.
echo [VS-ControlAPI] exited with code %RC%
echo Log: $apiLog
pause
"@
if ($DryRun) {
    Write-Host '[dry-run] start VS-ControlAPI via node.exe only (LIVE)'
} else {
    $apiLauncher = Join-Path $env:TEMP 'vs-v2-VS-ControlAPI-live-node-only.cmd'
    Set-Content -LiteralPath $apiLauncher -Value $apiCmd -Encoding ASCII
    $apiProc = Start-Process -FilePath 'cmd.exe' -ArgumentList @('/k', "`"$apiLauncher`"") -WorkingDirectory $Root -PassThru -WindowStyle Normal
    Set-Content -LiteralPath (Join-Path $logs 'control-api.live.log.pid') -Value $apiProc.Id
    Write-Ok "VS-ControlAPI CMD opened pid=$($apiProc.Id) (LIVE node-only)"
}

# --- 2) Market Core LIVE (CapitalOrderGateway bound) ---
$exe = Get-MarketCoreExe -Root $Root
if ($SkipMarketCore) {
    Write-Warn 'SkipMarketCore set - Market Core CMD will not open'
} elseif (-not $exe -and -not $DryRun) {
    throw 'market-core binary not found. Run Install.bat first.'
} else {
    $modeArg = '--mode LIVE'
    $mcPath = if ($exe) { $exe } else { 'market-core.exe' }
    Start-LiveLoggedProcess -Title 'VS-MarketCore' -FilePath $mcPath -Arguments $modeArg `
        -WorkingDirectory $Root -LogName 'market-core.live.log' -ExtraEnv @{
            OPERATING_MODE       = 'LIVE'
            LIVE_TRADING_ENABLED = 'true'
            MARKET_CORE_BRIDGE   = $(if ($env:MARKET_CORE_BRIDGE) { $env:MARKET_CORE_BRIDGE } else { 'true' })
        } | Out-Null
    Write-Ok 'market-core --mode LIVE (Capital order gateway bound when creds/auth OK)'
}

# --- 3) Dashboard (admin Control Panel :5173) ---
$sysNode = Get-SystemNodeExe
$npmCli = Get-SystemNpmCliJs
Write-Ok 'dashboard via node + system npm-cli.js (bypass npm.ps1 prefix bug)'
Write-Host "  node: $sysNode"
Write-Host "  npm:  $npmCli"
Start-LiveLoggedProcess -Title 'VS-Dashboard' -FilePath $sysNode `
    -Arguments ('"{0}" run dev --workspace=@vs-v2/dashboard' -f $npmCli) `
    -WorkingDirectory $Root -LogName 'dashboard.live.log' -ExtraEnv @{
        OPERATING_MODE       = 'LIVE'
        LIVE_TRADING_ENABLED = 'true'
        npm_config_prefix    = ''
        VITE_API_URL         = ''
        API_ADMIN_TOKEN      = $(if ($env:API_ADMIN_TOKEN) { $env:API_ADMIN_TOKEN } else { '' })
    } | Out-Null

# --- 4) Public Client Web gateway (:5174) ---
Write-Step 'Public Client Web (dist-client + client-gateway :5174)'
$clientDist = Ensure-ClientWebDist -Root $Root -DryRun:$DryRun
$env:CLIENT_DIST = $clientDist
$env:CLIENT_PANEL_DIST = $clientDist
$gatewayJs = Join-Path $Root 'apps\dashboard\client-gateway.mjs'
if (-not (Test-Path -LiteralPath $gatewayJs) -and -not $DryRun) {
    throw "Missing $gatewayJs"
}
Start-LiveLoggedProcess -Title 'VS-ClientWeb' -FilePath $sysNode `
    -Arguments ('"{0}"' -f $gatewayJs) `
    -WorkingDirectory (Join-Path $Root 'apps\dashboard') -LogName 'client-web.live.log' -ExtraEnv @{
        OPERATING_MODE         = 'LIVE'
        LIVE_TRADING_ENABLED   = 'true'
        CLIENT_PUBLIC_PORT     = $(if ($env:CLIENT_PUBLIC_PORT) { $env:CLIENT_PUBLIC_PORT } else { '5174' })
        CLIENT_DIST            = $clientDist
        CLIENT_PANEL_DIST      = $clientDist
        CONTROL_API_HOST       = '127.0.0.1'
        CONTROL_API_PORT       = '3000'
        CLIENT_COOKIE_SECURE   = $(if ($env:CLIENT_COOKIE_SECURE) { $env:CLIENT_COOKIE_SECURE } else { 'true' })
        TRUST_PROXY            = $(if ($env:TRUST_PROXY) { $env:TRUST_PROXY } else { 'true' })
        CLIENT_CORS_ORIGIN     = $(if ($env:CLIENT_CORS_ORIGIN) { $env:CLIENT_CORS_ORIGIN } else { '' })
    } | Out-Null
Write-Ok 'VS-ClientWeb gateway on :5174 (public client login via access_code)'

Write-Ok 'All LIVE service CMD windows launched (API + Market Core + Dashboard + Client Web)'

$apiBase = if ($env:CONTROL_API_URL) { $env:CONTROL_API_URL.TrimEnd('/') } else { 'http://127.0.0.1:3000' }
if (-not $DryRun) {
    Write-Step 'Waiting for Control API /health (windows already open)'
    Write-Host "  $apiBase/health"
    Write-Host '  Watch the VS-ControlAPI window or logs\control-api.live.log'
    if (-not (Wait-HttpOk -Url "$apiBase/health" -Attempts 90 -DelayMs 1000 -Label 'Control API /health')) {
        Write-LogTail -Path $apiLog -Lines 60
        throw "Control API did not become healthy at $apiBase/health - see VS-ControlAPI window / log tail. Fix .env.live DB_* then re-run LIVE.bat or Restart-ControlAPI.bat."
    }
    Write-Ok 'Control API healthy'
    $missingRoutes = @(Test-ControlApiCriticalRoutes -ApiBase $apiBase)
    if ($missingRoutes.Count -gt 0) {
        Write-Warn ("Critical API routes missing: {0} - forcing control-api rebuild + restart" -f ($missingRoutes -join ', '))
        try {
            $conns = Get-NetTCPConnection -LocalPort 3000 -State Listen -ErrorAction SilentlyContinue
            foreach ($c in @($conns)) {
                if ($c.OwningProcess) {
                    Stop-Process -Id $c.OwningProcess -Force -ErrorAction SilentlyContinue
                }
            }
        } catch { }
        Start-Sleep -Seconds 1
        $distJs = Ensure-ControlApiDist -Root $Root -Force
        if (-not $DryRun) {
            $apiLauncher = Join-Path $env:TEMP 'vs-v2-VS-ControlAPI-live-node-only.cmd'
            if (Test-Path -LiteralPath $apiLauncher) {
                $apiProc = Start-Process -FilePath 'cmd.exe' -ArgumentList @('/k', "`"$apiLauncher`"") -WorkingDirectory $Root -PassThru -WindowStyle Normal
                Set-Content -LiteralPath (Join-Path $logs 'control-api.live.log.pid') -Value $apiProc.Id
            }
            if (-not (Wait-HttpOk -Url "$apiBase/health" -Attempts 90 -DelayMs 1000 -Label 'Control API /health (after rebuild)')) {
                throw 'Control API unhealthy after forced rebuild - see VS-ControlAPI window'
            }
            $missingRoutes2 = @(Test-ControlApiCriticalRoutes -ApiBase $apiBase)
            if ($missingRoutes2.Count -gt 0) {
                throw ("FEED/NEWS routes still missing after rebuild: {0}. Close all VS-* windows, run Install.bat, then LIVE.bat." -f ($missingRoutes2 -join ', '))
            }
            Write-Ok 'FEED probe + NEWS desk routes verified after rebuild'
        }
    } else {
        Write-Ok 'FEED probe + NEWS desk routes verified'
    }
    Invoke-LivePreflight -Root $Root
}

$dashUrl = Resolve-DashboardUrl
$controlUrl = ($dashUrl.TrimEnd('/') + '/control')
# Always probe LOCAL gateway for health - never a stale trycloudflare.com from a previous run.
$clientLocalUrl = Resolve-ClientWebLocalUrl
$clientUrl = Resolve-ClientWebUrl
if (-not $DryRun) {
    Write-Step 'Waiting for Dashboard'
    if (-not (Wait-HttpOk -Url $dashUrl -Attempts 60 -DelayMs 500 -Label 'Dashboard')) {
        Write-Warn "Dashboard not responding yet at $dashUrl (check VS-Dashboard window / logs\dashboard.live.log)"
    } else {
        Write-Ok "Dashboard up $dashUrl"
    }
    Write-Step 'Waiting for Client Web gateway (local)'
    if (-not (Wait-HttpOk -Url $clientLocalUrl -Attempts 40 -DelayMs 500 -Label 'Client Web :5174')) {
        Write-Warn "Client Web not responding yet at $clientLocalUrl (check VS-ClientWeb / logs\client-web.live.log)"
    } else {
        Write-Ok "Client Web up $clientLocalUrl"
    }
    if (-not (Wait-HttpOk -Url "$apiBase/health" -Attempts 5 -DelayMs 500 -Label 'Control API recheck' -Quiet)) {
        Write-LogTail -Path $apiLog -Lines 80
        throw "Control API died after start. See VS-ControlAPI window / $apiLog. Or run Restart-ControlAPI.bat"
    }

    # Public HTTPS for remote clients - optional; must not abort LIVE.
    Write-Step 'Public Cloudflare URL for clients (optional)'
    $publicUrl = $null
    try {
        $publicUrl = Start-ClientWebCloudflareTunnel -Root $Root -DryRun:$DryRun
    } catch {
        Write-Warn ("Cloudflare start failed - LIVE continues locally: {0}" -f $_.Exception.Message)
    }
    if ($publicUrl) {
        $clientUrl = ($publicUrl.TrimEnd('/') + '/')
        Write-Ok "Clients copy URL: $clientUrl"
    } else {
        Write-Warn 'No public Cloudflare URL yet - Control Panel / local :5174 still work. Paste tunnel URL on Clients if needed.'
    }
}

if (-not $NoBrowser -and -not $DryRun) {
    Write-Step 'Opening Control Panel in browser'
    Start-Process $controlUrl
} elseif ($DryRun) {
    Write-Host "[dry-run] would open browser $controlUrl"
}

Assert-LiveArmed

if (-not $DryRun) {
    try { Invoke-LivePreflight -Root $Root } catch { Write-Warn "Final LIVE preflight: $($_.Exception.Message)" }
}

Write-Host ''
Write-Host '============================================================' -ForegroundColor Red
Write-Host '  LIVE stack running (started by LIVE.bat)' -ForegroundColor Red
Write-Host '  CMD: VS-ControlAPI | VS-MarketCore | VS-Dashboard | VS-ClientWeb | VS-Cloudflare' -ForegroundColor Red
Write-Host "  Control Panel: $controlUrl" -ForegroundColor Red
Write-Host "  Client Web (local): http://127.0.0.1:$(if ($env:CLIENT_PUBLIC_PORT) { $env:CLIENT_PUBLIC_PORT } else { '5174' })/" -ForegroundColor Red
Write-Host "  Client public URL:  $clientUrl" -ForegroundColor Yellow
Write-Host "  Control API: $(if ($env:CONTROL_API_URL) { $env:CONTROL_API_URL } else { 'http://127.0.0.1:3000' })" -ForegroundColor Red
Write-Host '  Mode: LIVE | Live trading: true | Broker orders: ARMED' -ForegroundColor Red
Write-Host "  Logs: $logs\*.live.log" -ForegroundColor Red
Write-Host "  Launch log: $launchLog" -ForegroundColor Red
if ($clientUrl -match 'trycloudflare\.com' -or ($clientUrl -match '^https://' -and $clientUrl -notmatch '127\.0\.0\.1|localhost')) {
    Write-Host '  Copy Client public URL above (also Control Panel -> Clients -> COPY URL)' -ForegroundColor Yellow
} else {
    Write-Host '  No Cloudflare URL yet: install cloudflared or paste tunnel URL on Clients page' -ForegroundColor Yellow
}
Write-Host '============================================================' -ForegroundColor Red
try { Stop-Transcript | Out-Null } catch {}
return

} catch {
    Write-Host ''
    Write-Host '[FAIL] LIVE start aborted' -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    if ($_.ScriptStackTrace) {
        Write-Host $_.ScriptStackTrace -ForegroundColor DarkRed
    }
    if ($launchLog) {
        Write-Host "Full log: $launchLog" -ForegroundColor Yellow
    }
    try { Stop-Transcript | Out-Null } catch {}
    exit 1
}
