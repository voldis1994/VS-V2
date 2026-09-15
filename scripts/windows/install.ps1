# VS-V2 first-time Windows install (Install.bat)
# deps / npm install / C++ build / DB migrate. Does NOT start LIVE or send orders.
param(
    [string]$RepoRoot = '',
    [switch]$DryRun,
    [switch]$SkipCppBuild,
    [switch]$SkipDocker
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$Root = Get-VsRoot -Hint $RepoRoot
Set-Location $Root
Assert-VsRepoRoot -Root $Root

Write-Host ''
Write-Host '============================================================' -ForegroundColor Green
Write-Host '  VS-V2 Install.bat — first-time setup (PAPER only)' -ForegroundColor Green
Write-Host '============================================================' -ForegroundColor Green
Write-Host "  Root: $Root"
Write-Host '  LIVE trading will NOT be started.'
Write-Host ''

if ($DryRun) { Write-Warn 'DRY RUN — checks only; no installs/builds' }

Write-Step 'Checking dependencies'
[void](Ensure-Tool -Name 'git' -WingetId 'Git.Git' -Required -DryRun:$DryRun)
[void](Ensure-Tool -Name 'node' -WingetId 'OpenJS.NodeJS.LTS' -Required -DryRun:$DryRun)
[void](Ensure-Tool -Name 'npm' -WingetId 'OpenJS.NodeJS.LTS' -Required -DryRun:$DryRun)
[void](Ensure-Tool -Name 'cmake' -WingetId 'Kitware.CMake' -Required:(-not $SkipCppBuild) -DryRun:$DryRun)
if (-not $SkipDocker) {
    [void](Ensure-Tool -Name 'docker' -WingetId 'Docker.DockerDesktop' -Required -DryRun:$DryRun)
}

$nodeMajor = 0
try { $nodeMajor = [int]((node -v) -replace '^v', '').Split('.')[0] } catch {}
if (-not $DryRun -and $nodeMajor -lt 20) {
    throw "Node.js 20+ required (found major=$nodeMajor)"
}
if ($nodeMajor -ge 20) { Write-Ok "Node.js major=$nodeMajor" }

Write-Step 'Preparing PAPER env (fail-closed)'
$envPaper = Join-Path $Root '.env.paper'
$envFile = Join-Path $Root '.env'
$examplePaper = Join-Path $Root '.env.paper.example'
$example = Join-Path $Root '.env.example'

if (-not (Test-Path -LiteralPath $envPaper) -and (Test-Path -LiteralPath $examplePaper)) {
    if ($DryRun) { Write-Host '[dry-run] copy .env.paper.example -> .env.paper' }
    else { Copy-Item -LiteralPath $examplePaper -Destination $envPaper }
    Write-Ok 'created .env.paper'
}
if (-not (Test-Path -LiteralPath $envFile)) {
    $src = $null
    if (Test-Path -LiteralPath $examplePaper) { $src = $examplePaper }
    elseif (Test-Path -LiteralPath $example) { $src = $example }
    if ($src) {
        if ($DryRun) { Write-Host "[dry-run] copy $src -> .env" }
        else { Copy-Item -LiteralPath $src -Destination $envFile }
        Write-Ok 'created .env'
    }
}

if (Test-Path -LiteralPath $envPaper) { Import-DotEnvFile -Path $envPaper }
elseif (Test-Path -LiteralPath $envFile) { Import-DotEnvFile -Path $envFile }
Enforce-PaperFailClosed
Assert-PaperFailClosed
Write-Ok 'OPERATING_MODE=PAPER LIVE_TRADING_ENABLED=false (forced)'

function Set-EnvKey([string]$File, [string]$Key, [string]$Value) {
    if ($DryRun) {
        Write-Host "[dry-run] upsert $Key=$Value in $File"
        return
    }
    if (-not (Test-Path -LiteralPath $File)) {
        Set-Content -LiteralPath $File -Value "$Key=$Value`r`n" -Encoding UTF8
        return
    }
    $raw = Get-Content -LiteralPath $File -Raw
    if ($null -eq $raw) { $raw = '' }
    $pattern = '(?m)^' + [regex]::Escape($Key) + '=.*$'
    if ($raw -match $pattern) {
        $raw = [regex]::Replace($raw, $pattern, "$Key=$Value")
    } else {
        if ($raw.Length -gt 0 -and -not $raw.EndsWith("`n")) { $raw += "`r`n" }
        $raw += "$Key=$Value`r`n"
    }
    Set-Content -LiteralPath $File -Value $raw -NoNewline -Encoding UTF8
}

if (Test-Path -LiteralPath $envPaper) {
    Set-EnvKey $envPaper 'OPERATING_MODE' 'PAPER'
    Set-EnvKey $envPaper 'LIVE_TRADING_ENABLED' 'false'
}
if (Test-Path -LiteralPath $envFile) {
    Set-EnvKey $envFile 'OPERATING_MODE' 'PAPER'
    Set-EnvKey $envFile 'LIVE_TRADING_ENABLED' 'false'
}

Write-Step 'npm install (workspaces)'
if ($DryRun) { Write-Host '[dry-run] npm install' }
else {
    & npm install
    if ($LASTEXITCODE -ne 0) { throw 'npm install failed' }
    Write-Ok 'npm install complete'
}

Write-Step 'Build control-api (TypeScript)'
if ($DryRun) { Write-Host '[dry-run] npm run build --workspace=@vs-v2/control-api' }
else {
    & npm run build --workspace=@vs-v2/control-api
    if ($LASTEXITCODE -ne 0) { throw 'control-api build failed' }
    Write-Ok 'control-api built'
}

if (-not $SkipDocker) {
    Write-Step 'Database (postgres + redis)'
    Start-DockerDeps -Root $Root -DryRun:$DryRun
}

Write-Step 'DB migrations'
if ($DryRun) { Write-Host '[dry-run] npm run migrate --workspace=@vs-v2/control-api' }
else {
    & npm run migrate --workspace=@vs-v2/control-api
    if ($LASTEXITCODE -ne 0) { throw 'DB migrate failed' }
    Write-Ok 'migrations applied'
}

if (-not $SkipCppBuild) {
    Write-Step 'C++ market-core build'
    if ($DryRun) { Write-Host '[dry-run] cmake configure + build market-core' }
    else {
        $cmake = Resolve-Tool -Name 'cmake'
        if (-not $cmake) {
            throw 'cmake not found on PATH after Ensure-Tool (close window and re-run Install.bat)'
        }
        $usedPreset = $false
        if (Test-Path -LiteralPath (Join-Path $Root 'CMakePresets.json')) {
            & $cmake --preset windows-release
            if ($LASTEXITCODE -eq 0) {
                & $cmake --build --preset windows-release --target market-core -j
                $usedPreset = ($LASTEXITCODE -eq 0)
            }
        }
        if (-not $usedPreset) {
            $buildDir = Join-Path $Root 'build'
            if (-not (Test-Path -LiteralPath $buildDir)) {
                New-Item -ItemType Directory -Path $buildDir | Out-Null
            }
            & $cmake -B $buildDir -DMR_BUILD_TESTS=OFF
            if ($LASTEXITCODE -ne 0) { throw 'cmake configure failed' }
            & $cmake --build $buildDir --target market-core -j
            if ($LASTEXITCODE -ne 0) { throw 'cmake build market-core failed' }
        }
        $exe = Get-MarketCoreExe -Root $Root
        if (-not $exe) { throw 'market-core binary not found after build' }
        Write-Ok "market-core built: $exe"
    }
} else {
    Write-Warn 'SkipCppBuild set — market-core build skipped'
}

Write-Step 'Installation verification (no LIVE start)'
Assert-PaperFailClosed
$checks = @(
    [pscustomobject]@{ name = 'package.json'; ok = (Test-Path (Join-Path $Root 'package.json')) }
    [pscustomobject]@{ name = 'node_modules'; ok = ((Test-Path (Join-Path $Root 'node_modules')) -or $DryRun) }
    [pscustomobject]@{ name = 'control-api'; ok = (Test-Path (Join-Path $Root 'apps\control-api\package.json')) }
    [pscustomobject]@{ name = 'dashboard'; ok = (Test-Path (Join-Path $Root 'apps\dashboard\package.json')) }
    [pscustomobject]@{ name = 'market-core binary'; ok = ([bool](Get-MarketCoreExe -Root $Root) -or $SkipCppBuild -or $DryRun) }
)
$failed = @($checks | Where-Object { -not $_.ok })
foreach ($c in $checks) {
    if ($c.ok) { Write-Ok $c.name } else { Write-Fail $c.name }
}
if ($failed.Count -gt 0) {
    throw ('Install verification failed: ' + (($failed | ForEach-Object name) -join ', '))
}

$marker = Join-Path $Root '.vs-v2-installed'
if (-not $DryRun) {
    @"
installed_at=$(Get-Date -Format o)
operating_mode=PAPER
live_trading_enabled=false
"@ | Set-Content -LiteralPath $marker -Encoding UTF8
}
Write-Ok 'wrote .vs-v2-installed marker'

Write-Host ''
Write-Host '============================================================' -ForegroundColor Green
Write-Host '  INSTALL COMPLETE — PAPER only, LIVE not started' -ForegroundColor Green
Write-Host '  Next: double-click V2.bat for daily PAPER launch' -ForegroundColor Green
Write-Host '============================================================' -ForegroundColor Green
exit 0
