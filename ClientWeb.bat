@echo off
setlocal EnableExtensions
REM Build + start public Client Web gateway only (:5174).
REM Does NOT arm LIVE trading. Use when Control API is already running.
cd /d "%~dp0"
set "ROOT=%CD%"
title VS-V2 Client Web
color 0B

echo.
echo ============================================================
echo   ClientWeb.bat - public client panel gateway :5174
echo ============================================================
echo.

if not exist "%ROOT%\scripts\windows\deploy-client-web.ps1" (
  color 0C
  echo [FAIL] Missing scripts\windows\deploy-client-web.ps1
  pause
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\windows\deploy-client-web.ps1" -RepoRoot "%ROOT%" %*
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
  color 0C
  echo [FAIL] ClientWeb failed code %RC%
  pause
  exit /b %RC%
)
echo.
echo Client Web should be at http://127.0.0.1:5174/
pause
exit /b 0
