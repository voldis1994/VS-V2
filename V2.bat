@echo off
setlocal EnableExtensions EnableDelayedExpansion
REM VS-V2 daily Windows launcher.
REM Starts Control API + C++ Market Core (--mode PAPER) + Dashboard, opens browser.
REM Does NOT reinstall. Does NOT switch SHADOW/LIVE. No broker orders.

cd /d "%~dp0"
set "ROOT=%CD%"
title VS-V2 daily PAPER launch
color 0A

echo.
echo ============================================================
echo   VS-V2  V2.bat  -  daily PAPER launch (one-shot)
echo   Opens 3 CMD windows: Control API + Market Core + Dashboard
echo   LIVE trading OFF - broker orders forbidden
echo ============================================================
echo   Folder: %ROOT%
echo.

if not exist "%ROOT%\package.json" (
  color 0C
  echo [FAIL] Run V2.bat from the VS-V2 repo root.
  pause
  exit /b 1
)
if not exist "%ROOT%\scripts\windows\start-v2.ps1" (
  color 0C
  echo [FAIL] Missing scripts\windows\start-v2.ps1 - run Install.bat / pull latest.
  pause
  exit /b 1
)

set "OPERATING_MODE=PAPER"
set "LIVE_TRADING_ENABLED=false"

where powershell >nul 2>&1
if errorlevel 1 (
  color 0C
  echo [FAIL] PowerShell is required.
  pause
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\windows\start-v2.ps1" -RepoRoot "%ROOT%" %*
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
  color 0C
  echo.
  echo [FAIL] V2.bat failed with exit code %RC%
  pause
  exit /b %RC%
)

echo.
echo PAPER stack running. Keep the 3 service CMD windows open.
echo   VS-ControlAPI / VS-MarketCore / VS-Dashboard
echo   For real Capital open/close: close this stack and run LIVE.bat
pause
exit /b 0
