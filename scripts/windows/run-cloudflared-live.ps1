# VS-V2 Cloudflare quick tunnel runner (ASCII-only for Windows PowerShell 5.1).
# Shows trycloudflare.com URL in this window, writes marker, pushes Control API.
# Optional public HTTPS only - failures here must NEVER abort LIVE.bat.
param(
    [Parameter(Mandatory = $true)][string]$RepoRoot,
    [Parameter(Mandatory = $true)][string]$CloudflaredExe,
    [Parameter(Mandatory = $true)][string]$TargetUrl,
    [Parameter(Mandatory = $true)][string]$LogPath
)

$ErrorActionPreference = 'Continue'
try { $Host.UI.RawUI.WindowTitle = 'VS-Cloudflare' } catch { }
Set-Location -LiteralPath $RepoRoot
$env:VS_V2_ROOT = $RepoRoot

$marker = Join-Path $RepoRoot '.vs-v2-client-public-url'
$urlTxt = Join-Path $RepoRoot 'logs\client-public-url.txt'
$logsDir = Join-Path $RepoRoot 'logs'
if (-not (Test-Path -LiteralPath $logsDir)) {
    New-Item -ItemType Directory -Path $logsDir | Out-Null
}

Write-Host ''
Write-Host '============================================================' -ForegroundColor Cyan
Write-Host '  VS-Cloudflare quick tunnel' -ForegroundColor Cyan
Write-Host "  Target: $TargetUrl" -ForegroundColor Cyan
Write-Host "  Log:    $LogPath" -ForegroundColor Cyan
Write-Host '  Keep this window open while clients use the public URL.' -ForegroundColor Cyan
Write-Host '  Waiting for https://....trycloudflare.com ...' -ForegroundColor Yellow
Write-Host '  iPhone: open the https URL (not 127.0.0.1). Keep this window open.' -ForegroundColor Yellow
Write-Host '============================================================' -ForegroundColor Cyan
Write-Host ''

$rx = [regex]'https://[a-zA-Z0-9.-]+\.trycloudflare\.com'
$script:FoundUrl = $null
$apiBase = if ($env:CONTROL_API_URL) { $env:CONTROL_API_URL.TrimEnd('/') } else { 'http://127.0.0.1:3000' }

function Save-PublicUrl([string]$Url) {
    $clean = $Url.Trim().TrimEnd('/')
    try {
        Set-Content -LiteralPath $marker -Value "$clean`n" -Encoding utf8
        Set-Content -LiteralPath $urlTxt -Value "$clean`n" -Encoding utf8
    } catch {
        Write-Host "[WARN] Could not write marker: $($_.Exception.Message)" -ForegroundColor Yellow
    }
    $env:CLIENT_PUBLIC_URL = $clean

    Write-Host ''
    Write-Host '============================================================' -ForegroundColor Green
    Write-Host '  CLIENT PUBLIC URL (copy this for clients):' -ForegroundColor Green
    Write-Host "  $clean" -ForegroundColor Yellow
    Write-Host '  iPhone Safari: paste https URL, wait 5s, reload once if needed' -ForegroundColor Green
    Write-Host '  Also: Control Panel -> Clients -> REFRESH URL -> COPY URL' -ForegroundColor Green
    Write-Host "  Saved: $marker" -ForegroundColor Green
    Write-Host '============================================================' -ForegroundColor Green
    Write-Host ''

    try {
        $headers = @{ 'Content-Type' = 'application/json' }
        if ($env:API_ADMIN_TOKEN -and $env:API_ADMIN_TOKEN -ne 'CHANGE_ME_ADMIN_TOKEN') {
            $headers['x-admin-token'] = $env:API_ADMIN_TOKEN
        }
        $body = (@{ url = $clean } | ConvertTo-Json -Compress)
        Invoke-RestMethod -Method Put -Uri "$apiBase/api/system/client-web" -Headers $headers -Body $body -TimeoutSec 8 | Out-Null
        Write-Host '[OK] URL pushed to Control Panel Clients page' -ForegroundColor Green
    } catch {
        Write-Host "[WARN] Could not push to API yet: $($_.Exception.Message)" -ForegroundColor Yellow
        Write-Host '       Open Clients -> REFRESH URL (marker file is already saved).' -ForegroundColor Yellow
    }
}

function Receive-CloudflaredLine([string]$Line) {
    if ([string]::IsNullOrWhiteSpace($Line)) { return }
    Write-Host $Line
    if ($script:FoundUrl) { return }
    $m = $rx.Match($Line)
    if ($m.Success) {
        $script:FoundUrl = $m.Value.TrimEnd('/')
        Save-PublicUrl -Url $script:FoundUrl
    }
}

function Invoke-CloudflaredAttempt([string[]]$ExtraArgs) {
    if (Test-Path -LiteralPath $LogPath) {
        Remove-Item -LiteralPath $LogPath -Force -ErrorAction SilentlyContinue
    }
    New-Item -ItemType File -Path $LogPath -Force | Out-Null

    $argLine = (@('tunnel', '--no-autoupdate') + $ExtraArgs + @('--url', $TargetUrl)) -join ' '
    # Quote exe path for cmd
    $exeQ = '"' + $CloudflaredExe + '"'
    $cmd = "($exeQ $argLine) 1>> `"$LogPath`" 2>&1"
    Write-Host ("Running: {0} {1}" -f $CloudflaredExe, $argLine) -ForegroundColor DarkCyan

    $p = Start-Process -FilePath 'cmd.exe' -ArgumentList @('/c', $cmd) `
        -WorkingDirectory $RepoRoot -PassThru -WindowStyle Hidden

    $reader = $null
    try {
        $reader = [System.IO.File]::Open($LogPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        $sr = New-Object System.IO.StreamReader($reader)
        while (-not $p.HasExited) {
            while (-not $sr.EndOfStream) {
                Receive-CloudflaredLine -Line $sr.ReadLine()
            }
            Start-Sleep -Milliseconds 400
        }
        Start-Sleep -Milliseconds 300
        while (-not $sr.EndOfStream) {
            Receive-CloudflaredLine -Line $sr.ReadLine()
        }
        $sr.Close()
    } catch {
        Write-Host "[WARN] log reader: $($_.Exception.Message)" -ForegroundColor Yellow
        try {
            Get-Content -LiteralPath $LogPath -ErrorAction SilentlyContinue | ForEach-Object { Receive-CloudflaredLine -Line "$_" }
        } catch { }
    } finally {
        try { if ($reader) { $reader.Dispose() } } catch { }
    }

    $code = 1
    try { $code = [int]$p.ExitCode } catch { $code = 1 }
    return $code
}

$attempts = @(
    ,@( '--protocol', 'http2', '--edge-ip-version', '4' ),
    ,@( '--protocol', 'http2' ),
    ,@()
)

$rc = 1
foreach ($extra in $attempts) {
    if ($script:FoundUrl) { break }
    $label = if ($extra.Count -gt 0) { ($extra -join ' ') } else { '(default)' }
    Write-Host "Attempt: $label" -ForegroundColor Cyan
    $rc = Invoke-CloudflaredAttempt -ExtraArgs $extra
    if ($script:FoundUrl) { break }

    $tail = ''
    if (Test-Path -LiteralPath $LogPath) {
        $tail = ((Get-Content -LiteralPath $LogPath -Tail 20 -ErrorAction SilentlyContinue) -join ' ')
    }
    if ($extra.Count -gt 0 -and ($tail -match 'unknown flag|invalid argument|incorrect usage|not a valid|Unrecognized')) {
        Write-Host '[WARN] flags rejected - falling back' -ForegroundColor Yellow
        continue
    }
    if ($extra.Count -gt 0 -and $rc -ne 0) {
        Write-Host "[WARN] exit=$rc - falling back" -ForegroundColor Yellow
        continue
    }
    break
}

# If URL was found, cloudflared may have exited (failure) or still running via cmd /c finished.
# For a healthy tunnel cmd /c blocks until cloudflared exits - so when we get URL and then
# process still runs, Invoke-CloudflaredAttempt only returns after tunnel ends.
# That is intended: this window stays alive with the tunnel.

Add-Content -LiteralPath $LogPath -Value ("[{0}] finished code={1} url={2}" -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $rc, $script:FoundUrl) -ErrorAction SilentlyContinue
Write-Host ''
if ($script:FoundUrl) {
    Write-Host "[VS-Cloudflare] tunnel session ended. URL was: $($script:FoundUrl)" -ForegroundColor Yellow
} else {
    Write-Host "[VS-Cloudflare] no public URL (code=$rc). LIVE can still run on localhost." -ForegroundColor Red
    Write-Host "Log: $LogPath" -ForegroundColor Yellow
}
Write-Host 'Press any key to close...'
try { $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown') } catch { Start-Sleep -Seconds 30 }
