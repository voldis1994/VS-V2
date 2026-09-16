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
Write-Host '  VS-V2 Install.bat - first-time setup (PAPER only)' -ForegroundColor Green
Write-Host '============================================================' -ForegroundColor Green
Write-Host "  Root: $Root"
Write-Host '  LIVE trading will NOT be started.'
Write-Host ''

if ($DryRun) { Write-Warn 'DRY RUN - checks only; no installs/builds' }

Write-Step 'Checking dependencies'
[void](Ensure-Tool -Name 'git' -WingetId 'Git.Git' -Required -DryRun:$DryRun)
[void](Ensure-Tool -Name 'node' -WingetId 'OpenJS.NodeJS.LTS' -Required -DryRun:$DryRun)
[void](Ensure-Tool -Name 'npm' -WingetId 'OpenJS.NodeJS.LTS' -Required -DryRun:$DryRun)
[void](Ensure-Tool -Name 'cmake' -WingetId 'Kitware.CMake' -Required:(-not $SkipCppBuild) -DryRun:$DryRun)
[void](Ensure-Tool -Name 'ninja' -WingetId 'Ninja-build.Ninja' -Required:(-not $SkipCppBuild) -DryRun:$DryRun)
if (-not $SkipDocker) {
    [void](Ensure-Tool -Name 'docker' -WingetId 'Docker.DockerDesktop' -Required -DryRun:$DryRun)
}
# Optional: public Client Web tunnel (LIVE.bat uses this for trycloudflare.com URL)
[void](Ensure-Tool -Name 'cloudflared' -WingetId 'Cloudflare.cloudflared' -DryRun:$DryRun)
if (-not $SkipCppBuild) {
    Ensure-MsvcBuildTools -DryRun:$DryRun
    Enter-VsDevShell -DryRun:$DryRun
    [void](Ensure-Vcpkg -Root $Root -DryRun:$DryRun)
    # Re-assert local VCPKG_ROOT after Ensure-Vcpkg (VS installers often export their own).
    # Use Join-Path parts so path never depends on a literal "\v" sequence in source.
    $env:VCPKG_ROOT = Join-Path (Join-Path $Root 'tools') 'vcpkg'
    Remove-Item Env:CMAKE_TOOLCHAIN_FILE -ErrorAction SilentlyContinue
    Write-Ok "VCPKG_ROOT forced local: $env:VCPKG_ROOT"
}

$nodeMajor = 0
try { $nodeMajor = [int]((node -v) -replace '^v', '').Split('.')[0] } catch {}
if (-not $DryRun -and $nodeMajor -lt 20) {
    throw "Node.js 20+ required (found major=$nodeMajor)"
}
if ($nodeMajor -ge 20) { Write-Ok "Node.js major=$nodeMajor" }

Write-Step 'Preparing PAPER env (fail-closed) + LIVE env template'
$envPaper = Join-Path $Root '.env.paper'
$envLive = Join-Path $Root '.env.live'
$envFile = Join-Path $Root '.env'
$examplePaper = Join-Path $Root '.env.paper.example'
$exampleLive = Join-Path $Root '.env.live.example'
$example = Join-Path $Root '.env.example'

if (-not (Test-Path -LiteralPath $envPaper) -and (Test-Path -LiteralPath $examplePaper)) {
    if ($DryRun) { Write-Host '[dry-run] copy .env.paper.example -> .env.paper' }
    else { Copy-Item -LiteralPath $examplePaper -Destination $envPaper }
    Write-Ok 'created .env.paper'
}
if (-not (Test-Path -LiteralPath $envLive) -and (Test-Path -LiteralPath $exampleLive)) {
    if ($DryRun) { Write-Host '[dry-run] copy .env.live.example -> .env.live' }
    else { Copy-Item -LiteralPath $exampleLive -Destination $envLive }
    Write-Ok 'created .env.live (fill Capital creds before LIVE.bat)'
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


function Ensure-AdminToken {
    param([string]$EnvFile)
    if (-not (Test-Path -LiteralPath $EnvFile)) { return }
    $raw = Get-Content -LiteralPath $EnvFile -Raw
    if ($null -eq $raw) { $raw = '' }
    $needs = $true
    if ($raw -match '(?m)^API_ADMIN_TOKEN=(.+)$') {
        $cur = $Matches[1].Trim().Trim('"').Trim("'")
        if ($cur -and $cur -ne 'CHANGE_ME_ADMIN_TOKEN') { $needs = $false }
    }
    if (-not $needs) { return }
    $bytes = New-Object byte[] 24
    [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes)
    $token = ($bytes | ForEach-Object { $_.ToString('x2') }) -join ''
    Set-EnvKey $EnvFile 'API_ADMIN_TOKEN' $token
    # Never leave insecure admin enabled after we minted a real token.
    Set-EnvKey $EnvFile 'ALLOW_INSECURE_ADMIN' 'false'
    Write-Ok "generated API_ADMIN_TOKEN in $(Split-Path -Leaf $EnvFile)"
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

if (Test-Path -LiteralPath $envPaper) { Ensure-AdminToken -EnvFile $envPaper }
if (Test-Path -LiteralPath $envFile) { Ensure-AdminToken -EnvFile $envFile }

}

Write-Step 'npm install (workspaces)'
if ($DryRun) { Write-Host '[dry-run] npm install' }
else {
    $nodeExe = Get-SystemNodeExe
    $npmCli = Get-SystemNpmCliJs
    & $nodeExe $npmCli install
    if ($LASTEXITCODE -ne 0) { throw 'npm install failed' }
    Write-Ok 'npm install complete'
}

Write-Step 'Build control-api (TypeScript)'
if ($DryRun) { Write-Host '[dry-run] npm run build --workspace=@vs-v2/control-api' }
else {
    $distJs = Ensure-ControlApiDist -Root $Root -Force
    Assert-ControlApiDist -DistJs $distJs
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
    Write-Step 'C++ market-core build (forced local tools\vcpkg + VsDevCmd + Ninja)'
    if ($DryRun) { Write-Host '[dry-run] Invoke-MarketCoreBuild' }
    else {
        # Never let VS BuildTools vcpkg hijack the toolchain.
        $localVcpkg = Join-Path (Join-Path $Root 'tools') 'vcpkg'
        $env:VCPKG_ROOT = $localVcpkg
        [Environment]::SetEnvironmentVariable('VCPKG_ROOT', $localVcpkg, 'Process')
        Remove-Item Env:CMAKE_TOOLCHAIN_FILE -ErrorAction SilentlyContinue

        Enter-VsDevShell

        $cmake = Resolve-Tool -Name 'cmake'
        if (-not $cmake) { throw 'cmake not found on PATH' }
        $ninja = Resolve-Tool -Name 'ninja'
        if (-not $ninja) { throw 'ninja not found on PATH (winget Ninja-build.Ninja)' }
        $toolchain = Get-VcpkgToolchain -Root $Root
        if (-not $toolchain) {
            $toolchain = Join-Path $localVcpkg 'scripts\buildsystems\vcpkg.cmake'
        }
        if (-not (Test-Path -LiteralPath $toolchain)) {
            throw "Local vcpkg toolchain missing: $toolchain (Ensure-Vcpkg must run first)"
        }
        # Refuse Visual Studio's bundled vcpkg path explicitly.
        if ($toolchain -match 'Microsoft Visual Studio') {
            throw "Refusing VS bundled vcpkg toolchain: $toolchain"
        }

        Write-Ok "VCPKG_ROOT=$env:VCPKG_ROOT"
        Write-Ok "toolchain=$toolchain"
        Write-Ok "ninja=$ninja"

        Invoke-MarketCoreBuild -Root $Root -CMake $cmake -Ninja $ninja -Toolchain $toolchain

        $exe = Get-MarketCoreExe -Root $Root
        if (-not $exe) { throw 'market-core binary not found after build' }
        Write-Ok "market-core built: $exe"
    }
} else {
    Write-Warn 'SkipCppBuild set - market-core build skipped'
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
Write-Host '  INSTALL COMPLETE - LIVE not auto-started' -ForegroundColor Green
Write-Host '  Next: LIVE.bat (daily LIVE Capital orders - type LIVE to confirm)' -ForegroundColor Green
Write-Host '        or V2.bat (PAPER fail-closed, no broker orders)' -ForegroundColor Green
Write-Host '============================================================' -ForegroundColor Green
exit 0
