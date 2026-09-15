# Shared helpers for VS-V2 Windows Install.bat / V2.bat
# HARD RULES: OPERATING_MODE=PAPER, LIVE_TRADING_ENABLED=false, no broker orders.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-VsRoot {
    param([string]$Hint = '')
    if ($Hint -and (Test-Path -LiteralPath (Join-Path $Hint 'package.json'))) {
        return (Resolve-Path -LiteralPath $Hint).Path
    }
    return (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

function Write-Step([string]$Message) { Write-Host ''; Write-Host "==> $Message" -ForegroundColor Cyan }
function Write-Ok([string]$Message) { Write-Host "[OK] $Message" -ForegroundColor Green }
function Write-Warn([string]$Message) { Write-Host "[WARN] $Message" -ForegroundColor Yellow }
function Write-Fail([string]$Message) { Write-Host "[FAIL] $Message" -ForegroundColor Red }

function Assert-VsRepoRoot {
    param([string]$Root)
    foreach ($rel in @(
            'package.json',
            'apps\control-api\package.json',
            'apps\dashboard\package.json',
            'apps\market-core'
        )) {
        if (-not (Test-Path -LiteralPath (Join-Path $Root $rel))) {
            throw "Not a VS-V2 repo root (missing $rel)."
        }
    }
}

function Import-DotEnvFile {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return }
    Get-Content -LiteralPath $Path | ForEach-Object {
        $line = $_.Trim()
        if (-not $line -or $line.StartsWith('#')) { return }
        $eq = $line.IndexOf('=')
        if ($eq -lt 1) { return }
        $key = $line.Substring(0, $eq).Trim()
        $val = $line.Substring($eq + 1).Trim()
        if (($val.StartsWith('"') -and $val.EndsWith('"')) -or ($val.StartsWith("'") -and $val.EndsWith("'"))) {
            $val = $val.Substring(1, $val.Length - 2)
        }
        Set-Item -Path "Env:$key" -Value $val
        [Environment]::SetEnvironmentVariable($key, $val, 'Process')
    }
}

function Enforce-PaperFailClosed {
    $env:OPERATING_MODE = 'PAPER'
    $env:LIVE_TRADING_ENABLED = 'false'
    if (-not $env:MARKET_CORE_BRIDGE) { $env:MARKET_CORE_BRIDGE = 'true' }
    if (-not $env:CONTROL_API_URL) { $env:CONTROL_API_URL = 'http://127.0.0.1:3000' }
    if (-not $env:CAPITAL_BASE_URL) {
        $env:CAPITAL_BASE_URL = 'https://api-capital.backend-capital.com'
    }
    if ($env:CAPITAL_BASE_URL -match 'demo') {
        Write-Warn "CAPITAL_BASE_URL looks like demo ($($env:CAPITAL_BASE_URL)); PAPER expects LIVE market-data host"
    }
}

function Assert-PaperFailClosed {
    if (($env:OPERATING_MODE + '').ToUpperInvariant() -ne 'PAPER') {
        throw "Safety abort: OPERATING_MODE must be PAPER (got '$($env:OPERATING_MODE)')."
    }
    $live = ($env:LIVE_TRADING_ENABLED + '').ToLowerInvariant()
    if ($live -eq 'true' -or $live -eq '1') {
        throw 'Safety abort: LIVE_TRADING_ENABLED must be false.'
    }
}

function Test-CommandExists([string]$Name) {
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Update-SessionPath {
    # winget installs often update Machine/User PATH, but the current cmd/powershell
    # session keeps the old PATH — refresh so newly installed tools are visible.
    $machine = [Environment]::GetEnvironmentVariable('Path', 'Machine')
    $user = [Environment]::GetEnvironmentVariable('Path', 'User')
    if ($machine -or $user) {
        $env:Path = @($machine, $user) -join ';'
    }
}

function Find-ToolOnDisk {
    param([string]$Name)
    $exe = if ($Name -match '\.exe$') { $Name } else { "$Name.exe" }
    $dirs = @()

    if ($Name -match '^(cmake)(\.exe)?$') {
        $dirs += @(
            "${env:ProgramFiles}\CMake\bin",
            "${env:ProgramFiles(x86)}\CMake\bin",
            "${env:LOCALAPPDATA}\Programs\CMake\bin",
            "${env:ProgramFiles}\Kitware\CMake\bin"
        )
        # winget package layouts sometimes version the folder
        foreach ($root in @("${env:ProgramFiles}\CMake", "${env:ProgramFiles(x86)}\CMake", "${env:LOCALAPPDATA}\Programs")) {
            if (Test-Path -LiteralPath $root) {
                Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
                    ForEach-Object {
                        $bin = Join-Path $_.FullName 'bin'
                        if (Test-Path -LiteralPath (Join-Path $bin $exe)) { $dirs += $bin }
                    }
            }
        }
    } elseif ($Name -match '^(git)(\.exe)?$') {
        $dirs += @("${env:ProgramFiles}\Git\cmd", "${env:ProgramFiles(x86)}\Git\cmd")
    } elseif ($Name -match '^(node|npm)(\.exe)?$') {
        $dirs += @("${env:ProgramFiles}\nodejs", "${env:LOCALAPPDATA}\Programs\nodejs")
    } elseif ($Name -match '^(docker)(\.exe)?$') {
        $dirs += @(
            "${env:ProgramFiles}\Docker\Docker\resources\bin",
            "${env:ProgramFiles}\Docker\Docker\resources"
        )
    }

    foreach ($dir in ($dirs | Select-Object -Unique)) {
        $candidate = Join-Path $dir $exe
        if (Test-Path -LiteralPath $candidate) {
            if ($env:Path -notlike "*$dir*") {
                $env:Path = "$dir;$env:Path"
            }
            return $candidate
        }
    }
    return $null
}

function Resolve-Tool {
    param([string]$Name)
    Update-SessionPath
    if (Test-CommandExists $Name) {
        return (Get-Command $Name -ErrorAction SilentlyContinue).Source
    }
    return (Find-ToolOnDisk -Name $Name)
}

function Ensure-Tool {
    param(
        [string]$Name,
        [string]$WingetId,
        [switch]$Required,
        [switch]$DryRun
    )
    $resolved = Resolve-Tool -Name $Name
    if ($resolved) {
        Write-Ok "$Name present ($resolved)"
        return $true
    }
    Write-Warn "$Name missing"
    if ($DryRun) {
        Write-Host "  [dry-run] would winget install $WingetId (then refresh PATH / probe install dirs)"
        return (-not $Required)
    }
    if ($WingetId -and (Test-CommandExists 'winget')) {
        Write-Step "Installing $Name via winget ($WingetId)"
        $wingetArgs = @(
            'install', '-e', '--id', $WingetId,
            '--accept-package-agreements', '--accept-source-agreements',
            '--disable-interactivity'
        )
        & winget @wingetArgs
        $wingetCode = $LASTEXITCODE
        # 0 = success, -1978335189 (0x8A15002B) often means already installed
        if ($wingetCode -ne 0 -and $wingetCode -ne -1978335189) {
            Write-Warn "winget exit code $wingetCode for $WingetId (will still probe PATH/install dirs)"
        }
        Update-SessionPath
        Start-Sleep -Seconds 1
        $resolved = Resolve-Tool -Name $Name
        if ($resolved) {
            Write-Ok "$Name available after install ($resolved)"
            return $true
        }
    } elseif ($WingetId) {
        Write-Warn 'winget not found — cannot auto-install; install the tool manually'
    }

    if ($Required) {
        $hint = switch -Regex ($Name) {
            '^cmake' {
                'Install CMake (add to PATH), or: winget install -e --id Kitware.CMake — then close this window and re-run Install.bat'
            }
            default {
                "Install it, close this window, then re-run Install.bat. winget id: $WingetId"
            }
        }
        throw "Required tool missing: $Name. $hint"
    }
    return $false
}

function Get-MarketCoreExe {
    param([string]$Root)
    $candidates = @(
        (Join-Path $Root 'build\windows-release\apps\market-core\market-core.exe'),
        (Join-Path $Root 'build\windows-debug\apps\market-core\market-core.exe'),
        (Join-Path $Root 'build\apps\market-core\Release\market-core.exe'),
        (Join-Path $Root 'build\apps\market-core\Debug\market-core.exe'),
        (Join-Path $Root 'build\apps\market-core\market-core.exe'),
        (Join-Path $Root 'build\apps\market-core\market-core')
    )
    foreach ($c in $candidates) {
        if (Test-Path -LiteralPath $c) { return $c }
    }
    return $null
}

function Wait-HttpOk {
    param([string]$Url, [int]$Attempts = 40, [int]$DelayMs = 500)
    for ($i = 0; $i -lt $Attempts; $i++) {
        try {
            $resp = Invoke-WebRequest -Uri $Url -UseBasicParsing -TimeoutSec 2
            if ($resp.StatusCode -ge 200 -and $resp.StatusCode -lt 300) { return $true }
        } catch {
            Start-Sleep -Milliseconds $DelayMs
        }
    }
    return $false
}

function Invoke-PaperPreflight {
    param([string]$Root, [switch]$RequireCapital)
    Assert-PaperFailClosed
    $api = if ($env:CONTROL_API_URL) { $env:CONTROL_API_URL.TrimEnd('/') } else { 'http://127.0.0.1:3000' }

    if (-not (Wait-HttpOk -Url "$api/health" -Attempts 2 -DelayMs 200)) {
        throw "control-api /health unreachable at $api"
    }
    Write-Ok 'control-api /health'

    try {
        $pf = Invoke-RestMethod -Uri "$api/api/system/preflight" -TimeoutSec 5
        if (-not $pf.ok) {
            throw ("preflight fail-closed failed: " + ($pf.reasons | ConvertTo-Json -Compress))
        }
        if ($pf.live_trading_enabled -eq $true) {
            throw 'Safety abort: preflight reports live_trading_enabled=true'
        }
        if ($pf.live_entries_allowed -eq $true) {
            throw 'Safety abort: preflight reports live_entries_allowed=true'
        }
        if (($pf.operating_mode + '').ToUpperInvariant() -ne 'PAPER') {
            throw "Safety abort: preflight operating_mode=$($pf.operating_mode)"
        }
        if ($pf.broker_orders_forbidden -ne $true) {
            Write-Warn 'preflight missing broker_orders_forbidden=true'
        } else {
            Write-Ok 'preflight PAPER fail-closed (broker orders forbidden)'
        }
        if ($RequireCapital -and -not $pf.data_ready) {
            throw 'Capital LIVE market-data not ready (PAPER_REQUIRE_CAPITAL=1)'
        }
        if (-not $pf.data_ready) {
            Write-Warn 'Capital credentials incomplete — market-core may idle without LIVE market data (still no orders)'
        }
    } catch {
        if ($_.Exception.Message -match 'Safety abort|preflight fail|Capital LIVE|unreachable') { throw }
        Write-Warn '/api/system/preflight unavailable; checking /api/system/status'
        $st = Invoke-RestMethod -Uri "$api/api/system/status" -TimeoutSec 5
        $mode = ($st.mode + '').ToUpperInvariant()
        if ($mode -ne 'PAPER' -or $st.live_enabled -eq $true -or $st.live_entries_allowed -eq $true) {
            throw "status not PAPER fail-closed: mode=$mode live=$($st.live_enabled) entries=$($st.live_entries_allowed)"
        }
        Write-Ok 'status PAPER fail-closed'
    }
}

function Start-DockerDeps {
    param([string]$Root, [switch]$DryRun)
    $compose = Join-Path $Root 'infra\docker\docker-compose.yml'
    if (-not (Test-Path -LiteralPath $compose)) { throw "Missing $compose" }
    if (-not (Test-CommandExists 'docker')) {
        throw 'Docker not found. Install Docker Desktop, start it, then re-run.'
    }
    if ($DryRun) {
        Write-Host '[dry-run] docker compose -f infra/docker/docker-compose.yml up -d postgres redis'
        return
    }
    Push-Location $Root
    try {
        & docker compose -f 'infra\docker\docker-compose.yml' up -d postgres redis
        if ($LASTEXITCODE -ne 0) {
            & docker-compose -f 'infra\docker\docker-compose.yml' up -d postgres redis
        }
        if ($LASTEXITCODE -ne 0) { throw 'Failed to start postgres/redis via docker compose' }
        Write-Ok 'postgres + redis started'
        Start-Sleep -Seconds 3
    } finally {
        Pop-Location
    }
}

function Resolve-DashboardUrl {
    if ($env:VITE_DEV_SERVER_URL) { return $env:VITE_DEV_SERVER_URL }
    return 'http://127.0.0.1:5173'
}
