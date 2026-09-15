@echo off
setlocal EnableExtensions EnableDelayedExpansion
REM =============================================================================
REM  VS.bat - QUARANTINED (legacy LIVE launcher)
REM  Do NOT use this file for daily work. It previously forced LIVE trading and
REM  pulled a remote bat from GitHub. That path is disabled for safety.
REM  Use:  Install.bat  (first-time PAPER setup)
REM        V2.bat       (daily PAPER start)
REM =============================================================================
cd /d "%~dp0"
set "ROOT=%CD%"
title VS.bat QUARANTINED - use V2.bat (PAPER)
color 0C

echo.
echo ============================================================
echo   VS.bat is QUARANTINED
echo ============================================================
echo   This legacy launcher forced OPERATING_MODE=LIVE and
echo   LIVE_TRADING_ENABLED=true, and could download a remote bat.
echo.
echo   Safe PAPER launchers:
echo     Install.bat  - first-time deps / build / DB
echo     V2.bat       - daily PAPER Control API + market-core + dashboard
echo.
echo   To arm LIVE deliberately, use the dashboard runtime-mode switch
echo   (confirm + health gates) after a PAPER start - never VS.bat.
echo ============================================================
echo.

REM Optional: allow an explicit break-glass only when BOTH flags are set.
REM Still does NOT pull remote scripts and does NOT auto-start LIVE.
if /I "%VS_ALLOW_LEGACY_BAT%"=="1" if /I "%I_UNDERSTAND_LIVE_RISK%"=="1" (
  echo [WARN] Break-glass flags set - still refusing auto-LIVE start.
  echo [WARN] Start PAPER with V2.bat, then arm LIVE from the UI if required.
  exit /b 2
)

echo [FAIL] Refusing to start. Use V2.bat for PAPER.
pause
exit /b 1
