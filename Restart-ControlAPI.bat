@echo off
setlocal EnableExtensions
REM Restart only Control API (keeps Dashboard / Market Core running).
cd /d "%~dp0"
set "ROOT=%CD%"
title VS-V2 Restart Control API
color 0A

echo.
echo ============================================================
echo   Restart-ControlAPI.bat
echo   Restarts Control API on :3000 without killing Dashboard
echo   Mode AUTO: reads .vs-v2-runtime-mode (LIVE after LIVE.bat)
echo ============================================================
echo.

if not exist "%ROOT%\scripts\windows\restart-control-api.ps1" (
  color 0C
  echo [FAIL] Missing scripts\windows\restart-control-api.ps1
  pause
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\windows\restart-control-api.ps1" -RepoRoot "%ROOT%"
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
  color 0C
  echo [FAIL] Restart failed code %RC%
  pause
  exit /b %RC%
)
echo.
echo Control API should be healthy. Refresh browser /control/clients
pause
exit /b 0
