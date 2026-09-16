# VS-V2 Cloudflare quick tunnel runner (ASCII-only for Windows PowerShell 5.1).
# Shows trycloudflare.com URL in this window, writes marker, pushes Control API.
param(
    [Parameter(Mandatory = $true)][string]$RepoRoot,
    [Parameter(Mandatory = $true)][string]$CloudflaredExe,
    [Parameter(Mandatory = $true)][string]$TargetUrl,
    [Parameter(Mandatory = $true)][string]$LogPath
)

$ErrorActionPreference = 'Continue'
$Host.UI.RawUI.WindowTitle = 'VS-Cloudflare'
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
Write-Host '============================================================' -ForegroundColor Cyan
Write-Host ''

if (Test-Path -LiteralPath $LogPath) {
    Remove-Item -LiteralPath $LogPath -Force -ErrorAction SilentlyContinue
}

$rx = [regex]'https://[a-zA-Z0-9.-]+\.trycloudflare\.com'
$found = $null
$apiBase = if ($env:CONTROL_API_URL) { $env:CONTROL_API_URL.TrimEnd('/') } else { 'http://127.0.0.1:3000' }

function Save-PublicUrl([string]$Url) {
    $clean = $Url.Trim().TrimEnd('/')
    Set-Content -LiteralPath $marker -Value "$clean`n" -Encoding utf8
    Set-Content -LiteralPath $urlTxt -Value "$clean`n" -Encoding utf8
    $env:CLIENT_PUBLIC_URL = $clean
    try {
        $origin = ([Uri]$clean).GetLeftPart([UriPartial]::Authority)
        $parts = @()
        if ($env:CLIENT_CORS_ORIGIN) {
            $parts = @($env:CLIENT_CORS_ORIGIN.Split(',') | ForEach-Object { $_.Trim() } | Where-Object { $_ })
        }
        if (-not ($parts | Where-Object { $_.ToLowerInvariant() -eq $origin.ToLowerInvariant() })) {
            $parts += $origin
            $env:CLIENT_CORS_ORIGIN = ($parts -join ',')
        }
    } catch { }

    Write-Host ''
    Write-Host '============================================================' -ForegroundColor Green
    Write-Host '  CLIENT PUBLIC URL (copy this for clients):' -ForegroundColor Green
    Write-Host "  $clean" -ForegroundColor Yellow
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

# cloudflared prints the URL on stderr; merge streams and tee to window + log.
& $CloudflaredExe tunnel --no-autoupdate --url $TargetUrl 2>&1 | ForEach-Object {
    $line = "$_"
    Add-Content -LiteralPath $LogPath -Value $line -Encoding utf8
    Write-Host $line
    if (-not $found) {
        $m = $rx.Match($line)
        if ($m.Success) {
            $found = $m.Value.TrimEnd('/')
            Save-PublicUrl -Url $found
        }
    }
}

$rc = $LASTEXITCODE
Add-Content -LiteralPath $LogPath -Value ("[{0}] cloudflared exited code={1}" -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $rc)
Write-Host ''
Write-Host "[VS-Cloudflare] exited with code $rc" -ForegroundColor Red
if (-not $found) {
    Write-Host 'No trycloudflare.com URL was detected. Check the log file above.' -ForegroundColor Yellow
}
Write-Host 'Press any key to close...'
try { $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown') } catch { Start-Sleep -Seconds 30 }
