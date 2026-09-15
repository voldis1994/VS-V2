# VS-V2 daily Windows launch (V2.bat)
# ONE double-click starts everything:
#   1) postgres + redis (docker)
#   2) Control API  -> visible CMD window
#   3) Market Core  -> visible CMD window (--mode PAPER)
#   4) Dashboard    -> visible CMD window
#   5) browser
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
Write-Host '  VS-V2 V2.bat - daily PAPER launch (one-shot)' -ForegroundColor Green
Write-Host '============================================================' -ForegroundColor Green
Write-Host "  Root: $Root"
Write-Host '  Opens 3 CMD windows: Control API + Market Core + Dashboard'
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

# Opens a visible CMD window that stays open (/k). All PAPER services are started this way
# so one V2.bat double-click is enough - no manual extra terminals.
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

    # Absolute paths only. Bare "npm.cmd" can resolve to a broken project-local shim
    # that looks for <repo>\node_modules\npm\bin\npm-cli.js (MODULE_NOT_FOUND).
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
        'OPERATING_MODE', 'LIVE_TRADING_ENABLED',
        'DB_HOST', 'DB_PORT', 'DB_NAME', 'DB_USER', 'DB_PASSWORD',
        'REDIS_URL', 'REDIS_HOST', 'REDIS_PORT',
        'CONTROL_API_HOST', 'CONTROL_API_PORT', 'CONTROL_API_URL',
        'DOTENV_CONFIG_PATH',
        'API_ADMIN_TOKEN', 'ALLOW_INSECURE_ADMIN',
        'CORS_ORIGIN', 'CLIENT_CORS_ORIGIN', 'TRUST_PROXY',
        'CAPITAL_API_KEY', 'CAPITAL_API_PASSWORD', 'CAPITAL_IDENTIFIER', 'CAPITAL_EPIC', 'CAPITAL_BASE_URL',
        'MASTER_ENCRYPTION_KEY', 'JWT_SECRET', 'PIPELINE_TOKEN'
    )
    $envBlock = @(
        'set OPERATING_MODE=PAPER',
        'set LIVE_TRADING_ENABLED=false',
        # Prevent npm.ps1/npm.cmd from using repo as prefix (MODULE_NOT_FOUND npm-cli.js).
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
    # Run via a tiny PS1 so stdout shows in the CMD window AND is appended to the log,
    # while preserving Node/npm exit codes (plain cmd pipes hide exit 1).
    $exePs = $exe.Replace("'", "''")
    $logPs = $logPath.Replace("'", "''")
    $psScript = @"
`$ErrorActionPreference = 'Continue'
Write-Host "Running: $exe $Arguments"
& '$exePs' $Arguments 2>&1 | Tee-Object -FilePath '$logPs' -Append
exit `$LASTEXITCODE
"@
    $psFile = Join-Path $env:TEMP ('vs-v2-run-' + $Title + '.ps1')
    Set-Content -LiteralPath $psFile -Value $psScript -Encoding ASCII
    $runLine = 'powershell -NoProfile -ExecutionPolicy Bypass -File "' + $psFile + '"' 
    # Visible CMD: title + live tee-like note. /k keeps window open if process exits.
    $cmd = @"
@echo off
title $Title
color 0A
cd /d "$WorkingDirectory"
$($envBlock -join "`r`n")
echo ============================================================
echo   $Title
echo   PAPER only - LIVE trading OFF - no broker orders
echo   Log: $logPath
echo   Close this window to stop this service.
echo ============================================================
echo [%date% %time%] starting $Title>> "$logPath"
echo [%date% %time%] exe=$exe args=$Arguments>> "$logPath"
echo Starting: $exe $Arguments
$runLine
set "RC=%ERRORLEVEL%"
echo [%date% %time%] exited $Title code=%RC%>> "$logPath"
echo.
echo [$Title] exited with code %RC%
echo Log: $logPath
echo.
pause
"@
    $launcher = Join-Path $env:TEMP ("vs-v2-" + $Title + '.cmd')
    Set-Content -LiteralPath $launcher -Value $cmd -Encoding ASCII
    # Normal (visible) so one V2.bat click shows all service windows - no manual CMD needed.
    $p = Start-Process -FilePath 'cmd.exe' -ArgumentList @('/k', "`"$launcher`"") -WorkingDirectory $WorkingDirectory -PassThru -WindowStyle Normal
    Set-Content -LiteralPath (Join-Path $logs ($LogName + '.pid')) -Value $p.Id
    Write-Ok "$Title CMD opened pid=$($p.Id) log=$logPath"
    Write-Host "  exe: $exe $Arguments"
    return $p
}

Write-Step 'Starting all PAPER services (3 CMD windows)'

# --- 1) Control API via absolute node.exe (never npm) ---
$apiLog = Join-Path $logs 'control-api.paper.log'
$distJs = Join-Path $Root 'apps\control-api\dist\index.js'
$envPaper = Join-Path $Root '.env.paper'
$nodeExe = Get-SystemNodeExe
if (-not (Test-Path -LiteralPath $distJs)) {
    Write-Warn 'control-api dist missing - building once with node (tsc)'
    $tscJs = Join-Path $Root 'node_modules\typescript\bin\tsc'
    $apiPkg = Join-Path $Root 'apps\control-api'
    if ((Test-Path -LiteralPath $tscJs) -and -not $DryRun) {
        Push-Location $apiPkg
        try {
            & $nodeExe $tscJs -p (Join-Path $apiPkg 'tsconfig.json')
            if ($LASTEXITCODE -ne 0) { throw "tsc failed for control-api (exit $LASTEXITCODE)" }
            # tsc does not copy *.sql - migrations must land in dist/db/migrations
            $copyJs = Join-Path $apiPkg 'scripts\copy-migrations.mjs'
            if (Test-Path -LiteralPath $copyJs) {
                & $nodeExe $copyJs
                if ($LASTEXITCODE -ne 0) { throw 'copy-migrations.mjs failed' }
            } else {
                $srcMig = Join-Path $apiPkg 'src\db\migrations'
                $dstMig = Join-Path $apiPkg 'dist\db\migrations'
                New-Item -ItemType Directory -Force -Path $dstMig | Out-Null
                Copy-Item -Path (Join-Path $srcMig '*') -Destination $dstMig -Force
            }
        } finally { Pop-Location }
    }
    if (-not (Test-Path -LiteralPath $distJs) -and -not $DryRun) {
        throw 'apps\control-api\dist\index.js missing. Run Install.bat then V2.bat again.'
    }
}
# Do NOT use --env-file="..." on Windows cmd: quotes become part of the path and Node exits 1 immediately.
# DOTENV_CONFIG_PATH makes import 'dotenv/config' load .env.paper; bat also sets DB_* from Import-DotEnvFile.
$nodeArgs = '"' + $distJs + '"'
$apiExtra = @{
    OPERATING_MODE         = 'PAPER'
    LIVE_TRADING_ENABLED   = 'false'
    CONTROL_API_HOST       = '0.0.0.0'
    CONTROL_API_PORT       = '3000'
}
# Windows: force IPv4 loopback for Docker Postgres (localhost -> ::1 breaks API boot).
if (-not $env:DB_HOST -or $env:DB_HOST -eq 'localhost') {
    $apiExtra['DB_HOST'] = '127.0.0.1'
    $env:DB_HOST = '127.0.0.1'
}
if (-not $env:REDIS_HOST -or $env:REDIS_HOST -eq 'localhost') {
    $apiExtra['REDIS_HOST'] = '127.0.0.1'
    $env:REDIS_HOST = '127.0.0.1'
}
if (Test-Path -LiteralPath $envPaper) {
    $apiExtra['DOTENV_CONFIG_PATH'] = $envPaper
}
# Ensure SQL migrations exist under dist (tsc never copies *.sql).
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
if ($apiExtra.ContainsKey('DOTENV_CONFIG_PATH')) { Write-Host "  env:   DOTENV_CONFIG_PATH=$envPaper" }
Start-LoggedProcess -Title 'VS-ControlAPI' -FilePath $nodeExe `
    -Arguments $nodeArgs `
    -WorkingDirectory $Root -LogName 'control-api.paper.log' -ExtraEnv $apiExtra | Out-Null

# --- 2) Market Core (separate CMD) ---
$exe = Get-MarketCoreExe -Root $Root
if ($SkipMarketCore) {
    Write-Warn 'SkipMarketCore set - Market Core CMD will not open'
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

# --- 3) Dashboard (separate CMD) ---
# Dashboard via system npm-cli.js + node.exe (never npm.ps1 / broken prefix).
$sysNode = Get-SystemNodeExe
$npmCli = Get-SystemNpmCliJs
Write-Ok "dashboard via node + system npm-cli.js (bypass npm.ps1 prefix bug)"
Write-Host "  node: $sysNode"
Write-Host "  npm:  $npmCli"
Start-LoggedProcess -Title 'VS-Dashboard' -FilePath $sysNode `
    -Arguments ('"{0}" run dev --workspace=@vs-v2/dashboard' -f $npmCli) `
    -WorkingDirectory $Root -LogName 'dashboard.paper.log' -ExtraEnv @{
        OPERATING_MODE       = 'PAPER'
        LIVE_TRADING_ENABLED = 'false'
        npm_config_prefix    = ''
    } | Out-Null

Write-Ok 'All service CMD windows launched (Control API + Market Core + Dashboard)'

# Health waits AFTER all windows are open (one-shot UX).
$apiBase = if ($env:CONTROL_API_URL) { $env:CONTROL_API_URL.TrimEnd('/') } else { 'http://127.0.0.1:3000' }
if (-not $DryRun) {
    Write-Step 'Waiting for Control API /health (windows already open)'
    Write-Host "  $apiBase/health"
    Write-Host '  Watch the VS-ControlAPI window or logs\control-api.paper.log'
    if (-not (Wait-HttpOk -Url "$apiBase/health" -Attempts 90 -DelayMs 1000 -Label 'Control API /health')) {
        Write-LogTail -Path $apiLog -Lines 60
        throw "Control API did not become healthy at $apiBase/health - see VS-ControlAPI window / log tail (often DB password mismatch). Fix .env.paper DB_* then re-run V2.bat or Restart-ControlAPI.bat."
    }
    Write-Ok 'Control API healthy'
    Invoke-PaperPreflight -Root $Root
}

$dashUrl = Resolve-DashboardUrl
$controlUrl = ($dashUrl.TrimEnd('/') + '/control')
if (-not $DryRun) {
    Write-Step 'Waiting for Dashboard'
    if (-not (Wait-HttpOk -Url $dashUrl -Attempts 60 -DelayMs 500 -Label 'Dashboard')) {
        Write-Warn "Dashboard not responding yet at $dashUrl (check VS-Dashboard window / logs\dashboard.paper.log)"
    } else {
        Write-Ok "Dashboard up $dashUrl"
    }
    # Re-check API after dashboard is up - catch silent crash during race.
    if (-not (Wait-HttpOk -Url "$apiBase/health" -Attempts 5 -DelayMs 500 -Label 'Control API recheck' -Quiet)) {
        Write-LogTail -Path $apiLog -Lines 80
        throw "Control API died after start. See VS-ControlAPI window / $apiLog. Or run Restart-ControlAPI.bat"
    }
}

if (-not $NoBrowser -and -not $DryRun) {
    Write-Step 'Opening Control Panel in browser'
    Start-Process $controlUrl
} elseif ($DryRun) {
    Write-Host "[dry-run] would open browser $controlUrl"
}

Assert-PaperFailClosed

if (-not $DryRun) {
    try { Invoke-PaperPreflight -Root $Root } catch { Write-Warn "Final preflight: $($_.Exception.Message)" }
}

Write-Host ''
Write-Host '============================================================' -ForegroundColor Green
Write-Host '  PAPER stack running (started by one V2.bat click)' -ForegroundColor Green
Write-Host '  CMD windows: VS-ControlAPI | VS-MarketCore | VS-Dashboard' -ForegroundColor Green
Write-Host "  Dashboard: $dashUrl" -ForegroundColor Green
Write-Host "  Control Panel: $controlUrl" -ForegroundColor Green
Write-Host "  Control API: $(if ($env:CONTROL_API_URL) { $env:CONTROL_API_URL } else { 'http://127.0.0.1:3000' })" -ForegroundColor Green
Write-Host '  Mode: PAPER | Live trading: false | Broker orders: forbidden' -ForegroundColor Green
Write-Host "  Logs: $logs\*.paper.log" -ForegroundColor Green
Write-Host '  Keep the 3 service CMD windows open. Close a window to stop that service.' -ForegroundColor Yellow
Write-Host '  If Clients shows API unreachable: run Restart-ControlAPI.bat' -ForegroundColor Yellow
Write-Host '============================================================' -ForegroundColor Green
exit 0
