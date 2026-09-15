# VS-V2 daily Windows launch (V2.bat)
# Control API + Market Core (--mode PAPER) + Dashboard + browser.
# Does NOT reinstall. Does NOT switch SHADOW/LIVE. Does NOT send broker orders.
param(
    [string]$RepoRoot = '',
    [switch]$DryRun,
    [switch]$NoBrowser,
    [switch]$SkipMarketCore
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$Root = Get-VsRoot -Hint $RepoRoot
Set-Location $Root
Assert-VsRepoRoot -Root $Root

Write-Host ''
Write-Host '============================================================' -ForegroundColor Green
Write-Host '  VS-V2 V2.bat - daily PAPER launch' -ForegroundColor Green
Write-Host '============================================================' -ForegroundColor Green
Write-Host "  Root: $Root"
Write-Host '  Default mode: PAPER | LIVE trading: OFF | No broker orders'
Write-Host ''

$marker = Join-Path $Root '.vs-v2-installed'
if (-not (Test-Path -LiteralPath $marker)) {
    Write-Warn 'Install marker .vs-v2-installed missing - run Install.bat once first'
    if (-not $DryRun -and -not (Test-Path (Join-Path $Root 'node_modules'))) {
        throw 'node_modules missing. Run Install.bat before V2.bat.'
    }
}

if (Test-Path (Join-Path $Root '.env.paper')) { Import-DotEnvFile -Path (Join-Path $Root '.env.paper') }
elseif (Test-Path (Join-Path $Root '.env')) { Import-DotEnvFile -Path (Join-Path $Root '.env') }
Enforce-PaperFailClosed
Assert-PaperFailClosed
# Admin API token: dashboard Vite proxy injects x-admin-token from this process env.
if (-not $env:API_ADMIN_TOKEN -or $env:API_ADMIN_TOKEN -eq 'CHANGE_ME_ADMIN_TOKEN') {
    if ($env:ALLOW_INSECURE_ADMIN -ne 'true') {
        Write-Warn 'API_ADMIN_TOKEN is CHANGE_ME/empty - control-api will refuse admin routes. Re-run Install.bat or set ALLOW_INSECURE_ADMIN=true for local-only.'
    }
} else {
    Write-Ok 'API_ADMIN_TOKEN loaded for dashboard proxy'
}


Write-Ok 'Forced OPERATING_MODE=PAPER LIVE_TRADING_ENABLED=false'

$logs = Join-Path $Root 'logs'
if (-not (Test-Path -LiteralPath $logs)) {
    if ($DryRun) { Write-Host '[dry-run] mkdir logs' }
    else { New-Item -ItemType Directory -Path $logs | Out-Null }
}

Write-Step 'Ensuring postgres + redis (no reinstall)'
try {
    Start-DockerDeps -Root $Root -DryRun:$DryRun
} catch {
    Write-Warn $_.Exception.Message
    Write-Warn 'Continuing - if DB is already local, API may still work'
}

function Start-LoggedProcess {
    param(
        [string]$Title,
        [string]$FilePath,
        [string]$Arguments,
        [string]$WorkingDirectory,
        [string]$LogName,
        [hashtable]$ExtraEnv
    )
    if ($Arguments -match '(?i)--mode\s+(LIVE|SHADOW)') {
        throw "Safety abort: refused non-PAPER market-core mode: $Arguments"
    }
    if ($DryRun) {
        Write-Host "[dry-run] start $Title :: $FilePath $Arguments"
        return $null
    }
    foreach ($k in $ExtraEnv.Keys) {
        Set-Item -Path "Env:$k" -Value ([string]$ExtraEnv[$k])
        [Environment]::SetEnvironmentVariable($k, [string]$ExtraEnv[$k], 'Process')
    }
    # Propagate PAPER + DB/API secrets into the child cmd (inheritance is unreliable across /k launchers).
    $passKeys = @(
        'OPERATING_MODE', 'LIVE_TRADING_ENABLED',
        'DB_HOST', 'DB_PORT', 'DB_NAME', 'DB_USER', 'DB_PASSWORD',
        'REDIS_URL', 'REDIS_HOST', 'REDIS_PORT',
        'CONTROL_API_HOST', 'CONTROL_API_PORT', 'CONTROL_API_URL',
        'API_ADMIN_TOKEN', 'ALLOW_INSECURE_ADMIN',
        'CORS_ORIGIN', 'CLIENT_CORS_ORIGIN', 'TRUST_PROXY',
        'CAPITAL_API_KEY', 'CAPITAL_API_PASSWORD', 'CAPITAL_IDENTIFIER', 'CAPITAL_EPIC', 'CAPITAL_BASE_URL',
        'MASTER_ENCRYPTION_KEY', 'JWT_SECRET', 'PIPELINE_TOKEN'
    )
    $envBlock = @(
        'set OPERATING_MODE=PAPER',
        'set LIVE_TRADING_ENABLED=false'
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
    # .cmd/.bat must be invoked with CALL; quoting npm.cmd alone often no-ops.
    $runLine = if ($FilePath -match '(?i)\.(cmd|bat)$') {
        'call "{0}" {1} >> "{2}" 2>&1' -f $FilePath, $Arguments, $logPath
    } else {
        '"{0}" {1} >> "{2}" 2>&1' -f $FilePath, $Arguments, $logPath
    }
    $cmd = @"
@echo off
cd /d "$WorkingDirectory"
$($envBlock -join "`r`n")
echo [%date% %time%] starting $Title>> "$logPath"
$runLine
echo [%date% %time%] exited $Title code=%ERRORLEVEL%>> "$logPath"
"@
    $launcher = Join-Path $env:TEMP ("vs-v2-" + $Title + '.cmd')
    Set-Content -LiteralPath $launcher -Value $cmd -Encoding ASCII
    $p = Start-Process -FilePath 'cmd.exe' -ArgumentList @('/k', $launcher) -WorkingDirectory $WorkingDirectory -PassThru -WindowStyle Minimized
    Set-Content -LiteralPath (Join-Path $logs ($LogName + '.pid')) -Value $p.Id
    Write-Ok "$Title started pid=$($p.Id) log=$logPath"
    return $p
}


Write-Step 'Starting Control API'
$apiLog = Join-Path $logs 'control-api.paper.log'
$distJs = Join-Path $Root 'apps\control-api\dist\index.js'
$envPaper = Join-Path $Root '.env.paper'
$nodeExe = Resolve-Tool -Name 'node'

# Prefer direct node start (faster, clearer logs) when dist exists.
if ((Test-Path -LiteralPath $distJs) -and $nodeExe) {
    # Relative paths: WorkingDirectory is repo root. Node 20+ --env-file loads .env.paper
    # (dotenv/config alone only reads .env and would miss PAPER DB settings).
    $nodeArgs = if (Test-Path -LiteralPath $envPaper) {
        '--env-file=.env.paper apps\control-api\dist\index.js'
    } else {
        'apps\control-api\dist\index.js'
    }
    Write-Ok "starting control-api via node dist ($distJs)"
    Start-LoggedProcess -Title 'VS-ControlAPI' -FilePath $nodeExe `
        -Arguments $nodeArgs `
        -WorkingDirectory $Root -LogName 'control-api.paper.log' -ExtraEnv @{
            OPERATING_MODE       = 'PAPER'
            LIVE_TRADING_ENABLED = 'false'
            CONTROL_API_HOST     = '0.0.0.0'
            CONTROL_API_PORT     = '3000'
        } | Out-Null
} else {
    $apiScript = 'start'
    if (-not (Test-Path -LiteralPath $distJs)) {
        Write-Warn 'control-api dist missing - using npm run dev (run Install.bat for production start)'
        $apiScript = 'dev'
    }
    Start-LoggedProcess -Title 'VS-ControlAPI' -FilePath 'npm.cmd' `
        -Arguments ("run {0} --workspace=@vs-v2/control-api" -f $apiScript) `
        -WorkingDirectory $Root -LogName 'control-api.paper.log' -ExtraEnv @{
            OPERATING_MODE       = 'PAPER'
            LIVE_TRADING_ENABLED = 'false'
            CONTROL_API_HOST     = '0.0.0.0'
            CONTROL_API_PORT     = '3000'
        } | Out-Null
}

if (-not $DryRun) {
    $apiBase = if ($env:CONTROL_API_URL) { $env:CONTROL_API_URL.TrimEnd('/') } else { 'http://127.0.0.1:3000' }
    Write-Host "Control API log: $apiLog"
    Write-Host 'If this sits here, open the minimized VS-ControlAPI window or the log above.'
    if (-not (Wait-HttpOk -Url "$apiBase/health" -Attempts 90 -DelayMs 1000 -Label 'Control API /health')) {
        Write-LogTail -Path $apiLog -Lines 50
        throw "Control API did not become healthy at $apiBase/health - see log tail above (often DB password mismatch or dist missing). Fix .env.paper DB_* then: docker compose -f infra\docker\docker-compose.yml up -d postgres redis & V2.bat"
    }
    Write-Ok 'Control API healthy'
    Invoke-PaperPreflight -Root $Root
}

Write-Step 'Starting Market Core (--mode PAPER, execution disabled)'
$exe = Get-MarketCoreExe -Root $Root
if ($SkipMarketCore) {
    Write-Warn 'SkipMarketCore set'
} elseif (-not $exe -and -not $DryRun) {
    throw 'market-core binary not found. Run Install.bat first.'
} else {
    $modeArg = '--mode PAPER'
    $mcPath = if ($exe) { $exe } else { 'market-core.exe' }
    Start-LoggedProcess -Title 'VS-MarketCore' -FilePath $mcPath -Arguments $modeArg `
        -WorkingDirectory $Root -LogName 'market-core.paper.log' -ExtraEnv @{
            OPERATING_MODE       = 'PAPER'
            LIVE_TRADING_ENABLED = 'false'
        } | Out-Null
    Write-Ok 'market-core --mode PAPER (no broker order gateway)'
}

Write-Step 'Starting Dashboard'
Start-LoggedProcess -Title 'VS-Dashboard' -FilePath 'npm.cmd' `
    -Arguments 'run dev --workspace=@vs-v2/dashboard' `
    -WorkingDirectory $Root -LogName 'dashboard.paper.log' -ExtraEnv @{
        OPERATING_MODE       = 'PAPER'
        LIVE_TRADING_ENABLED = 'false'
    } | Out-Null

$dashUrl = Resolve-DashboardUrl
if (-not $DryRun) {
    if (-not (Wait-HttpOk -Url $dashUrl -Attempts 60 -DelayMs 500)) {
        Write-Warn "Dashboard not responding yet at $dashUrl (check logs\dashboard.paper.log)"
    } else {
        Write-Ok "Dashboard up $dashUrl"
    }
}

if (-not $NoBrowser -and -not $DryRun) {
    Write-Step 'Opening Dashboard in browser'
    Start-Process $dashUrl
} elseif ($DryRun) {
    Write-Host "[dry-run] would open browser $dashUrl"
}

Assert-PaperFailClosed

if (-not $DryRun) {
    try { Invoke-PaperPreflight -Root $Root } catch { Write-Warn "Final preflight: $($_.Exception.Message)" }
}

Write-Host ''
Write-Host '============================================================' -ForegroundColor Green
Write-Host '  PAPER stack running' -ForegroundColor Green
Write-Host "  Dashboard: $dashUrl" -ForegroundColor Green
Write-Host "  Control API: $($env:CONTROL_API_URL)" -ForegroundColor Green
Write-Host '  Mode: PAPER | Live trading: false | Broker orders: forbidden' -ForegroundColor Green
Write-Host "  Logs: $logs\*.paper.log" -ForegroundColor Green
Write-Host '  Keep VS-ControlAPI / VS-MarketCore / VS-Dashboard windows open' -ForegroundColor Yellow
Write-Host '============================================================' -ForegroundColor Green
exit 0
