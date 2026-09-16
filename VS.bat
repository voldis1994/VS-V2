@echo off
setlocal EnableExtensions EnableDelayedExpansion
REM =============================================================================
REM  VS.bat - QUARANTINED (legacy remote LIVE launcher)
REM  Do NOT use this file. It previously forced LIVE and pulled a remote bat.
REM  Use:  Install.bat  (first-time setup)
REM        LIVE.bat     (daily LIVE - type LIVE to confirm Capital orders)
REM        V2.bat       (PAPER fail-closed fallback)
REM =============================================================================
cd /d "%~dp0"
set "ROOT=%CD%"
title VS.bat QUARANTINED - use LIVE.bat
color 0C

echo.
echo ============================================================
echo   VS.bat is QUARANTINED
echo ============================================================
echo   This legacy launcher forced OPERATING_MODE=LIVE and
echo   LIVE_TRADING_ENABLED=true, and could download a remote bat.
echo.
echo   Use these launchers instead:
echo     Install.bat  - first-time deps / build / DB
echo     LIVE.bat     - daily LIVE (type LIVE to confirm broker orders)
echo     V2.bat       - PAPER fail-closed (no broker orders)
echo ============================================================
echo.

if /I "%VS_ALLOW_LEGACY_BAT%"=="1" if /I "%I_UNDERSTAND_LIVE_RISK%"=="1" (
  echo [WARN] Break-glass flags set - still refusing legacy VS.bat auto-start.
  echo [WARN] Use LIVE.bat (typed LIVE confirm) for Capital open/close.
  exit /b 2
)

echo [FAIL] Refusing to start. Use LIVE.bat for LIVE or V2.bat for PAPER.
pause
exit /b 1
