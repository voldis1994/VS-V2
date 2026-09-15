@echo off
setlocal EnableExtensions EnableDelayedExpansion
REM VS-V2 first-time Windows install.
REM Checks/installs deps, npm install, C++ build, DB setup + migrations.
REM Does NOT start LIVE trading and does NOT send broker orders.
REM Daily use after install: V2.bat

cd /d "%~dp0"
set "ROOT=%CD%"
title VS-V2 Install (PAPER only)
color 0A

echo.
echo ============================================================
echo   VS-V2 Install.bat  —  first-time setup
echo   OPERATING_MODE=PAPER  ^|  LIVE trading OFF
echo ============================================================
echo   Folder: %ROOT%
echo.

if not exist "%ROOT%\package.json" (
  color 0C
  echo [FAIL] Run Install.bat from the VS-V2 repo root.
  pause
  exit /b 1
)
if not exist "%ROOT%\apps\control-api\package.json" (
  color 0C
  echo [FAIL] Missing apps\control-api — wrong folder?
  pause
  exit /b 1
)
if not exist "%ROOT%\scripts\windows\install.ps1" (
  color 0C
  echo [FAIL] Missing scripts\windows\install.ps1
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

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\windows\install.ps1" -RepoRoot "%ROOT%" %*
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
  color 0C
  echo.
  echo [FAIL] Install.bat failed with exit code %RC%
  pause
  exit /b %RC%
)

echo.
echo Install finished. Start daily PAPER stack with V2.bat
pause
exit /b 0
