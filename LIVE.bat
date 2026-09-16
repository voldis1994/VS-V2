@echo off
setlocal EnableExtensions EnableDelayedExpansion
REM VS-V2 daily Windows LIVE launcher.
REM Starts Control API + Market Core LIVE + Dashboard + public Client Web (:5174).
REM Requires typing LIVE to confirm real broker orders. Does NOT reinstall.

cd /d "%~dp0"
set "ROOT=%CD%"
title VS-V2 daily LIVE launch
color 0C

echo.
echo ============================================================
echo   VS-V2  LIVE.bat  -  daily LIVE launch (Capital + Client Web)
echo   Opens 4 CMD windows:
echo     Control API + Market Core + Dashboard + Client Web :5174
echo   REAL broker open/close - type LIVE to confirm
echo ============================================================
echo   Folder: %ROOT%
echo.

if not exist "%ROOT%\package.json" (
  color 0C
  echo [FAIL] Run LIVE.bat from the VS-V2 repo root.
  pause
  exit /b 1
)
if not exist "%ROOT%\scripts\windows\start-live.ps1" (
  color 0C
  echo [FAIL] Missing scripts\windows\start-live.ps1 - pull latest / run Install.bat.
  pause
  exit /b 1
)

echo WARNING: This arms LIVE trading against Capital.com.
echo          Real money open/close orders become allowed when the stack is healthy.
echo          Client Web serves on :5174 (put HTTPS tunnel in front for remote clients).
echo.
echo Type: LIVE   (live / Live also OK — no extra spaces)
echo.
set "TYPED="
set /p "TYPED=Type LIVE to confirm real Capital broker orders: "

REM Trim spaces. "Live " / " live" must still match.
for /f "tokens=* delims= " %%A in ("!TYPED!") do set "TYPED=%%A"

REM Case-insensitive exact match via findstr (more reliable than if /I on some consoles).
echo(!TYPED!| findstr /I /X /C:"LIVE" >nul
if errorlevel 1 (
  color 0C
  echo [FAIL] Confirmation failed - refused to start LIVE.
  echo        You typed: [!TYPED!]
  echo        Need exactly: LIVE
  echo        Safe PAPER path: V2.bat
  pause
  exit /b 2
)

echo [OK] LIVE confirmed.
echo.
echo Starting PowerShell launcher (you should see red/cyan steps below)...
echo If nothing prints for ^>30s: open Docker Desktop, wait until running, then re-run.
echo Log will be: %ROOT%\logs\live-launch.log
echo.

set "OPERATING_MODE=LIVE"
set "LIVE_TRADING_ENABLED=true"

where powershell >nul 2>&1
if errorlevel 1 (
  color 0C
  echo [FAIL] PowerShell is required.
  pause
  exit /b 1
)

if not exist "%ROOT%\logs" mkdir "%ROOT%\logs" >nul 2>&1

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\windows\start-live.ps1" -RepoRoot "%ROOT%" -ConfirmLive %*
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
  color 0C
  echo.
  echo [FAIL] LIVE.bat failed with exit code %RC%
  echo        Read: %ROOT%\logs\live-launch.log
  if exist "%ROOT%\logs\live-launch.log" (
    echo ----- live-launch.log tail -----
    powershell -NoProfile -Command "Get-Content -LiteralPath '%ROOT%\logs\live-launch.log' -Tail 40"
    echo ----- end -----
  )
  pause
  exit /b %RC%
)

echo.
echo LIVE stack running. Keep the 4 service CMD windows open.
echo   VS-ControlAPI / VS-MarketCore / VS-Dashboard / VS-ClientWeb
echo   Client Web: http://127.0.0.1:5174/
echo   Control Panel: http://127.0.0.1:5173/control
echo   Launch log: %ROOT%\logs\live-launch.log
pause
exit /b 0
