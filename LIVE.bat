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
set "TYPED="
set /p "TYPED=Type LIVE to confirm real Capital broker orders: "
if /I not "!TYPED!"=="LIVE" (
  color 0C
  echo [FAIL] Confirmation failed - refused to start LIVE. (You typed: !TYPED!)
  echo        Safe PAPER path: V2.bat
  pause
  exit /b 2
)

set "OPERATING_MODE=LIVE"
set "LIVE_TRADING_ENABLED=true"

where powershell >nul 2>&1
if errorlevel 1 (
  color 0C
  echo [FAIL] PowerShell is required.
  pause
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\windows\start-live.ps1" -RepoRoot "%ROOT%" -ConfirmLive %*
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
  color 0C
  echo.
  echo [FAIL] LIVE.bat failed with exit code %RC%
  pause
  exit /b %RC%
)

echo.
echo LIVE stack running. Keep the 4 service CMD windows open.
echo   VS-ControlAPI / VS-MarketCore / VS-Dashboard / VS-ClientWeb
echo   Client Web: http://127.0.0.1:5174/
echo   Control Panel mode switch still requires typing LIVE for re-arm.
pause
exit /b 0
