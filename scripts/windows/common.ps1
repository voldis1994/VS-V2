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
    # session keeps the old PATH - refresh so newly installed tools are visible.
    $machine = [Environment]::GetEnvironmentVariable('Path', 'Machine')
    $user = [Environment]::GetEnvironmentVariable('Path', 'User')
    if ($machine -or $user) {
        $env:Path = @($machine, $user) -join ';'
    }
}

function Find-ToolOnDisk {
    param([string]$Name)
    # npm is npm.cmd (not npm.exe). Node is node.exe.
    if ($Name -match '^(npm)(\.cmd)?$') {
        $candidates = @('npm.cmd', 'npm.exe')
    } elseif ($Name -match '\.exe$') {
        $candidates = @($Name)
    } else {
        $candidates = @("$Name.exe")
    }
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
        foreach ($exeName in $candidates) {
            $candidate = Join-Path $dir $exeName
            if (Test-Path -LiteralPath $candidate) {
                if ($env:Path -notlike "*$dir*") {
                    $env:Path = "$dir;$env:Path"
                }
                return $candidate
            }
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


function Get-SystemNodeExe {
    $n = Resolve-Tool -Name 'node'
    if (-not $n) { $n = Join-Path ${env:ProgramFiles} 'nodejs\node.exe' }
    if (-not (Test-Path -LiteralPath $n)) {
        throw "node.exe not found. Install Node.js 20+ LTS (winget install OpenJS.NodeJS.LTS), open NEW cmd, re-run."
    }
    # Refuse repo-local shims
    if ($n -match '(?i)[\\/]node_modules[\\/]') {
        throw "Refusing project-local node shim: $n"
    }
    return $n
}

function Get-SystemNpmCliJs {
    # Bypass npm.cmd / npm.ps1 entirely. Those scripts call npm-prefix.js and can
    # resolve prefix to the repo (then look for <repo>\node_modules\npm\bin\npm-cli.js).
    $nodeDir = Split-Path -Parent (Get-SystemNodeExe)
    $cli = Join-Path $nodeDir 'node_modules\npm\bin\npm-cli.js'
    if (-not (Test-Path -LiteralPath $cli)) {
        throw "System npm-cli.js missing at $cli - repair Node.js install."
    }
    return $cli
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
        Write-Warn 'winget not found - cannot auto-install; install the tool manually'
    }

    if ($Required) {
        $hint = switch -Regex ($Name) {
            '^cmake' {
                'Install CMake (add to PATH), or: winget install -e --id Kitware.CMake - then close this window and re-run Install.bat'
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

function Test-MsvcAvailable {
    Update-SessionPath
    if (Test-CommandExists 'cl') { return $true }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { return $false }
    $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    return [bool]$install
}

function Ensure-MsvcBuildTools {
    param([switch]$DryRun)
    if (Test-MsvcAvailable) {
        Write-Ok 'MSVC C++ build tools present'
        return
    }
    Write-Warn 'MSVC C++ tools missing (needed to compile market-core)'
    if ($DryRun) {
        Write-Host '[dry-run] would winget install Microsoft.VisualStudio.2022.BuildTools (VCTools workload)'
        return
    }
    if (-not (Test-CommandExists 'winget')) {
        throw 'MSVC Build Tools missing and winget not found. Install "Desktop development with C++" (VS 2022 Build Tools), close this window, re-run Install.bat.'
    }
    Write-Step 'Installing Visual Studio 2022 Build Tools (C++ workload) - may take several minutes'
    $override = '--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
    & winget install -e --id Microsoft.VisualStudio.2022.BuildTools `
        --accept-package-agreements --accept-source-agreements --disable-interactivity `
        --override $override
    Update-SessionPath
    if (-not (Test-MsvcAvailable)) {
        throw 'MSVC C++ tools still missing after winget. Install Build Tools manually, open a NEW cmd window, re-run Install.bat.'
    }
    # Verify the toolchain is actually usable in this shell (vswhere alone is not enough).
    try {
        Enter-VsDevShell
    } catch {
        throw ("MSVC installed but cl.exe cannot be loaded: " + $_.Exception.Message)
    }
    Write-Ok 'MSVC C++ build tools available'
}

# Bootstrap Microsoft vcpkg so CMake can find fmt/spdlog/yaml-cpp/curl/openssl (Windows).
function Ensure-Vcpkg {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [switch]$DryRun
    )
    $toolchainRel = 'scripts\buildsystems\vcpkg.cmake'
    $local = Join-Path (Join-Path $Root 'tools') 'vcpkg'
    $toolchain = Join-Path $local $toolchainRel

    # ALWAYS use repo-local tools\vcpkg. Visual Studio sets VCPKG_ROOT to its own
    # copy under BuildTools\VC\vcpkg which breaks manifests / baselines.
    if ($env:VCPKG_ROOT -and $env:VCPKG_ROOT -ne $local) {
        Write-Warn "Ignoring external VCPKG_ROOT=$($env:VCPKG_ROOT) (VS BuildTools) - forcing $local"
    }

    if ($DryRun) {
        Write-Host "[dry-run] would bootstrap vcpkg at $local and set VCPKG_ROOT"
        $env:VCPKG_ROOT = $local
        return $local
    }

    $git = Resolve-Tool -Name 'git'
    if (-not $git) { throw 'git required to clone vcpkg' }

    # Broken shallow clones from earlier Install attempts cause:
    #   path versions/baseline.json exists on disk, but not in <builtin-baseline>
    # Re-clone cleanly if the tree looks incomplete.
    $needsClone = -not (Test-Path -LiteralPath $local)
    if (-not $needsClone) {
        $baselineOnDisk = Test-Path -LiteralPath (Join-Path $local 'versions\baseline.json')
        $hasExe = Test-Path -LiteralPath (Join-Path $local 'vcpkg.exe')
        # Prior Install used shallow clone + stale builtin-baseline -> broken. If baseline.json
        # is missing at HEAD or toolchain is missing, wipe and re-clone.
        if ((-not $baselineOnDisk) -or (-not (Test-Path -LiteralPath $toolchain))) {
            Write-Warn 'Repairing tools\vcpkg (incomplete/broken clone from earlier Install)'
            Remove-Item -LiteralPath $local -Recurse -Force -ErrorAction SilentlyContinue
            $needsClone = $true
        } elseif (-not $hasExe) {
            Write-Warn 'tools\vcpkg present but vcpkg.exe missing - will bootstrap'
        }
    }

    $baseline = $null
    $manifest = Join-Path $Root 'vcpkg.json'
    if (Test-Path -LiteralPath $manifest) {
        try {
            $json = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
            if ($json.'builtin-baseline') { $baseline = [string]$json.'builtin-baseline' }
        } catch {}
    }
    if (-not $baseline) { $baseline = '9e44ec0e9f247d77c230ced0ee66c76296837807' }

    if ($needsClone) {
        Write-Step "Fetching vcpkg@$baseline into $local (fmt and other C++ deps)"
        New-Item -ItemType Directory -Path $local -Force | Out-Null
        Push-Location $local
        try {
            if (-not (Test-Path -LiteralPath (Join-Path $local '.git'))) {
                & $git init | Out-Null
                & $git remote add origin https://github.com/microsoft/vcpkg.git
            }
            & $git fetch --depth 1 origin $baseline
            if ($LASTEXITCODE -ne 0) { throw "git fetch vcpkg@$baseline failed" }
            & $git checkout --force FETCH_HEAD
            if ($LASTEXITCODE -ne 0) { throw "git checkout vcpkg@$baseline failed" }
        } finally {
            Pop-Location
        }
    } else {
        # Ensure existing clone is on the manifest baseline commit when possible.
        Push-Location $local
        try {
            $head = (& $git rev-parse HEAD 2>$null)
            if ($head -and $baseline -and ($head.Trim().ToLowerInvariant() -ne $baseline.ToLowerInvariant())) {
                Write-Step "Checking out vcpkg baseline $baseline"
                & $git fetch --depth 1 origin $baseline 2>$null
                & $git checkout --force FETCH_HEAD 2>$null
            }
        } finally {
            Pop-Location
        }
    }

    $vcpkgExe = Join-Path $local 'vcpkg.exe'
    if (-not (Test-Path -LiteralPath $vcpkgExe)) {
        Write-Step 'Bootstrapping vcpkg.exe'
        $bootstrap = Join-Path $local 'bootstrap-vcpkg.bat'
        if (-not (Test-Path -LiteralPath $bootstrap)) {
            throw "Missing $bootstrap - delete tools\vcpkg and re-run Install.bat"
        }
        Push-Location $local
        try {
            & cmd.exe /c "bootstrap-vcpkg.bat -disableMetrics"
            if ($LASTEXITCODE -ne 0) { throw 'bootstrap-vcpkg.bat failed' }
        } finally {
            Pop-Location
        }
    }

    if (-not (Test-Path -LiteralPath $toolchain)) {
        throw "vcpkg toolchain missing: $toolchain"
    }

    # Manifest builtin-baseline is pinned in vcpkg.json and matched by the checkout above.

    $env:VCPKG_ROOT = $local
    [Environment]::SetEnvironmentVariable('VCPKG_ROOT', $local, 'Process')
    Write-Ok "VCPKG_ROOT=$local (fmt/spdlog/yaml-cpp/curl/openssl via manifest vcpkg.json)"
    return $local
}

function Get-VcpkgToolchain {
    param([string]$Root)
    # Never trust external VCPKG_ROOT (VS BuildTools ships its own broken copy).
    $local = Join-Path (Join-Path $Root 'tools') 'vcpkg'
    $toolchain = Join-Path $local 'scripts\buildsystems\vcpkg.cmake'
    if (Test-Path -LiteralPath $toolchain) { return $toolchain }
    return $null
}

# Import MSVC environment (cl.exe / link.exe) into this PowerShell process.
function Enter-VsDevShell {
    param([switch]$DryRun)
    if (Test-CommandExists 'cl') {
        Write-Ok 'MSVC compiler (cl) already on PATH'
        return
    }
    if ($DryRun) {
        Write-Host '[dry-run] would import VsDevCmd.bat / DevShell environment'
        return
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'vswhere.exe not found. Install VS 2022 Build Tools (C++), close window, re-run Install.bat.'
    }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if (-not $vsPath) {
        $vsPath = & $vswhere -latest -products * -property installationPath 2>$null
    }
    if (-not $vsPath) {
        throw 'MSVC VC Tools not installed. Install Desktop development with C++ (VS 2022 Build Tools), close window, re-run Install.bat.'
    }
    $vsPath = "$vsPath".Trim()

    Write-Step "Importing Visual Studio developer environment (x64) from $vsPath"

    $loaded = $false
    $devShell = Join-Path $vsPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
    if (Test-Path -LiteralPath $devShell) {
        try {
            Import-Module $devShell -ErrorAction Stop
            # Cmdlet name collides with this function - call via module qualification.
            Microsoft.VisualStudio.DevShell\Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=amd64 -host_arch=amd64' | Out-Null
            $loaded = $true
            Write-Ok 'DevShell module loaded'
        } catch {
            Write-Warn ("DevShell module failed: $($_.Exception.Message) - falling back to VsDevCmd.bat")
        }
    }

    if (-not $loaded) {
        $vsDevCmd = Join-Path $vsPath 'Common7\Tools\VsDevCmd.bat'
        if (-not (Test-Path -LiteralPath $vsDevCmd)) {
            throw "VsDevCmd.bat missing under $vsPath"
        }
        # CRITICAL: use CALL. Without CALL, cmd.exe never runs `set` after a .bat file.
        $tmp = Join-Path $env:TEMP ('vs-v2-env-' + [guid]::NewGuid().ToString('n') + '.txt')
        $batFile = Join-Path $env:TEMP ('vs-v2-vsdev-' + [guid]::NewGuid().ToString('n') + '.bat')
        $batch = @(
            '@echo off',
            "call `"$vsDevCmd`" -arch=amd64 -host_arch=amd64",
            'if errorlevel 1 exit /b 1',
            "set > `"$tmp`""
        ) -join "`r`n"
        Set-Content -LiteralPath $batFile -Value $batch -Encoding ASCII
        try {
            & cmd.exe /c "`"$batFile`""
            if ($LASTEXITCODE -ne 0) { throw "VsDevCmd.bat failed (exit $LASTEXITCODE)" }
            if (-not (Test-Path -LiteralPath $tmp)) { throw 'VsDevCmd env capture file missing' }
            Get-Content -LiteralPath $tmp | ForEach-Object {
                $eq = $_.IndexOf('=')
                if ($eq -lt 1) { return }
                $name = $_.Substring(0, $eq)
                $value = $_.Substring($eq + 1)
                [Environment]::SetEnvironmentVariable($name, $value, 'Process')
                Set-Item -Path "Env:$name" -Value $value -ErrorAction SilentlyContinue
            }
        } finally {
            Remove-Item -LiteralPath $batFile -Force -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
        }
    }

    Update-SessionPath

    $cl = $null
    try { $cl = (Get-Command cl -ErrorAction SilentlyContinue).Source } catch {}
    if (-not $cl) {
        $msvcRoot = Join-Path $vsPath 'VC\Tools\MSVC'
        if (Test-Path -LiteralPath $msvcRoot) {
            $clCandidate = Get-ChildItem -Path $msvcRoot -Filter cl.exe -Recurse -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -match '\\Hostx64\\x64\\cl\.exe$' } |
                Select-Object -First 1
            if ($clCandidate) {
                $dir = $clCandidate.Directory.FullName
                $env:Path = "$dir;$env:Path"
                $cl = $clCandidate.FullName
                Write-Warn "Injected MSVC Hostx64\\x64 onto PATH: $dir"
            }
        }
    }
    if (-not $cl) {
        throw 'cl.exe still not on PATH after VsDevCmd. Install VS 2022 Build Tools workload VCTools, reboot, open a NEW cmd, cd to repo, run Install.bat.'
    }
    Write-Ok "MSVC environment loaded ($cl)"
}




# Run cmake+ninja inside one cmd.exe session that has already called VsDevCmd.
# This avoids PowerShell PATH / CMAKE_CXX_COMPILER / wrong VCPKG_ROOT issues.
function Invoke-MarketCoreBuild {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$CMake,
        [Parameter(Mandatory = $true)][string]$Ninja,
        [Parameter(Mandatory = $true)][string]$Toolchain
    )

    $vcpkgRoot = Join-Path (Join-Path $Root 'tools') 'vcpkg'
    if (-not (Test-Path -LiteralPath $Toolchain)) {
        throw "Toolchain missing: $Toolchain"
    }
    if ("$Toolchain" -match 'Microsoft Visual Studio') {
        throw "Refusing VS bundled vcpkg toolchain: $Toolchain"
    }
    # Force local vcpkg for this process and child cmd.
    $env:VCPKG_ROOT = $vcpkgRoot
    [Environment]::SetEnvironmentVariable('VCPKG_ROOT', $vcpkgRoot, 'Process')
    Remove-Item Env:CMAKE_TOOLCHAIN_FILE -ErrorAction SilentlyContinue

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'vswhere.exe not found'
    }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if (-not $vsPath) {
        $vsPath = & $vswhere -latest -products * -property installationPath 2>$null
    }
    if (-not $vsPath) { throw 'Visual Studio Build Tools with MSVC not found' }
    $vsDevCmd = Join-Path $vsPath.Trim() 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path -LiteralPath $vsDevCmd)) { throw "VsDevCmd.bat missing: $vsDevCmd" }

    $buildDir = Join-Path $Root 'build'
    if (Test-Path -LiteralPath $buildDir) {
        Write-Warn 'Removing previous build\\ folder for clean configure'
        Remove-Item -LiteralPath $buildDir -Recurse -Force
    }

    $ninjaDir = Split-Path -Parent $Ninja
    $log = Join-Path $Root 'build-cmake.log'
    $bat = Join-Path $env:TEMP ('vs-v2-build-' + [guid]::NewGuid().ToString('n') + '.bat')

    $cmakeConfigure = (
        '"{0}" -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ' +
        '-DCMAKE_TOOLCHAIN_FILE="{1}" -DCMAKE_MAKE_PROGRAM="{2}" ' +
        '-DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_FEATURE_FLAGS=manifests -DMR_BUILD_TESTS=OFF'
    ) -f $CMake, $Toolchain, $Ninja
    $cmakeBuild = '"{0}" --build build --target market-core -j' -f $CMake

    $lines = @(
        '@echo off',
        'setlocal EnableExtensions',
        ('call "{0}" -arch=amd64 -host_arch=amd64' -f $vsDevCmd),
        'if errorlevel 1 exit /b 1',
        ('set "VCPKG_ROOT={0}"' -f $vcpkgRoot),
        'set CMAKE_TOOLCHAIN_FILE=',
        ('set "PATH={0};%PATH%"' -f $ninjaDir),
        ('cd /d "{0}"' -f $Root),
        'echo VCPKG_ROOT=%VCPKG_ROOT%',
        'where cl',
        'where ninja',
        $cmakeConfigure,
        'if errorlevel 1 exit /b 1',
        $cmakeBuild,
        'exit /b %ERRORLEVEL%'
    )
    Set-Content -LiteralPath $bat -Value ($lines -join "`r`n") -Encoding ASCII

    Write-Step 'Configuring and building market-core (cmd + VsDevCmd + local vcpkg + Ninja)'
    try {
        & cmd.exe /c "`"$bat`" > `"$log`" 2>&1"
        $code = $LASTEXITCODE
    } finally {
        Remove-Item -LiteralPath $bat -Force -ErrorAction SilentlyContinue
    }
    if ($code -ne 0) {
        if (Test-Path -LiteralPath $log) {
            Write-Warn 'cmake/build log (tail):'
            Get-Content -LiteralPath $log -Tail 60 | ForEach-Object { Write-Host $_ }
        }
        throw "market-core build failed (exit $code). See build-cmake.log. If tools\\vcpkg is wrong: rmdir /s /q tools\\vcpkg build & Install.bat"
    }
    Write-Ok 'market-core cmake build finished'
}

function Wait-HttpOk {
    param(
        [string]$Url,
        [int]$Attempts = 40,
        [int]$DelayMs = 500,
        [string]$Label = '',
        [switch]$Quiet
    )
    $name = if ($Label) { $Label } else { $Url }
    if (-not $Quiet) {
        Write-Host ("Waiting for {0} (up to {1}s)..." -f $name, [int](($Attempts * $DelayMs) / 1000))
    }
    for ($i = 0; $i -lt $Attempts; $i++) {
        try {
            $resp = Invoke-WebRequest -Uri $Url -UseBasicParsing -TimeoutSec 2
            if ($resp.StatusCode -ge 200 -and $resp.StatusCode -lt 300) {
                if (-not $Quiet) { Write-Ok ("{0} is up" -f $name) }
                return $true
            }
        } catch {
            # keep polling
        }
        if (-not $Quiet -and (($i + 1) % 5 -eq 0)) {
            Write-Host ("  ... still waiting ({0}/{1}) {2}" -f ($i + 1), $Attempts, $name)
        }
        Start-Sleep -Milliseconds $DelayMs
    }
    return $false
}

function Write-LogTail {
    param([string]$Path, [int]$Lines = 40)
    if (-not (Test-Path -LiteralPath $Path)) {
        Write-Warn ("Log not found: {0}" -f $Path)
        return
    }
    Write-Warn ("----- tail {0} -----" -f $Path)
    Get-Content -LiteralPath $Path -Tail $Lines | ForEach-Object { Write-Host $_ }
    Write-Warn ("----- end log -----")
}

function Wait-PostgresReady {
    param([int]$Attempts = 30, [int]$DelayMs = 1000)
    $dockerExe = Resolve-Tool -Name 'docker'
    if (-not $dockerExe) {
        Write-Warn 'docker not on PATH - skipping pg_isready wait'
        Start-Sleep -Seconds 3
        return
    }
    Write-Host 'Waiting for postgres to accept connections...'
    for ($i = 0; $i -lt $Attempts; $i++) {
        $dbUser = if ($env:DB_USER) { $env:DB_USER } else { 'market_reader' }
        $dbName = if ($env:DB_NAME) { $env:DB_NAME } else { 'market_reader' }
        & $dockerExe exec vs-v2-postgres pg_isready -U $dbUser -d $dbName 2>$null | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Ok 'postgres is ready'
            return
        }
        # fallback: container health
        $health = & $dockerExe inspect -f '{{.State.Health.Status}}' vs-v2-postgres 2>$null
        if (("$health").Trim() -eq 'healthy') {
            Write-Ok 'postgres container healthy'
            return
        }
        if ((($i + 1) % 5) -eq 0) {
            Write-Host ("  ... postgres not ready yet ({0}/{1})" -f ($i + 1), $Attempts)
        }
        Start-Sleep -Milliseconds $DelayMs
    }
    Write-Warn 'postgres did not become ready in time - Control API may fail migrations'
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
            Write-Warn 'Capital credentials incomplete - market-core may idle without LIVE market data (still no orders)'
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
    # Same class of bug as cmake: docker may be installed but missing from this session PATH.
    $dockerExe = Resolve-Tool -Name 'docker'
    if (-not $dockerExe) {
        throw 'Docker not found. Install Docker Desktop, start it, close this window, then re-run.'
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
        Wait-PostgresReady
    } finally {
        Pop-Location
    }
}

function Resolve-DashboardUrl {
    if ($env:VITE_DEV_SERVER_URL) { return $env:VITE_DEV_SERVER_URL }
    return 'http://127.0.0.1:5173'
}
