#!/usr/bin/env python3
"""Validate Windows Install.bat / V2.bat startup flows (safety + dry-run simulation).

Runs on Linux CI without Windows. Does not start LIVE trading or send broker orders.
"""
from __future__ import annotations

import os
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def must_contain(text: str, needles: list[str], label: str) -> list[str]:
    return [f"{label}: missing {n!r}" for n in needles if n not in text]


def must_not_match(text: str, patterns: list[str], label: str) -> list[str]:
    bad = []
    for pat in patterns:
        if re.search(pat, text, flags=re.IGNORECASE | re.MULTILINE):
            bad.append(f"{label}: forbidden pattern {pat!r}")
    return bad


def test_files_exist() -> list[str]:
    errs = []
    for rel in (
        "Install.bat",
        "V2.bat",
        "scripts/windows/common.ps1",
        "scripts/windows/install.ps1",
        "scripts/windows/start-v2.ps1",
    ):
        if not (ROOT / rel).is_file():
            errs.append(f"missing file: {rel}")
    return errs


def test_install_flow() -> list[str]:
    bat = read("Install.bat")
    ps1 = read("scripts/windows/install.ps1")
    common = read("scripts/windows/common.ps1")
    errs: list[str] = []
    errs += must_contain(
        bat,
        [
            "OPERATING_MODE=PAPER",
            "LIVE_TRADING_ENABLED=false",
            r"scripts\windows\install.ps1",
        ],
        "Install.bat",
    )
    errs += must_contain(
        ps1,
        [
            "Enforce-PaperFailClosed",
            "Assert-PaperFailClosed",
            "npm install",
            "migrate",
            "market-core",
            ".vs-v2-installed",
            "DryRun",
            "LIVE trading will NOT be started",
            "Start-DockerDeps",
            "Ensure-Tool",
        ],
        "install.ps1",
    )
    errs += must_contain(
        common,
        [
            "Enforce-PaperFailClosed",
            "Assert-PaperFailClosed",
            "Invoke-PaperPreflight",
            "broker_orders_forbidden",
            "Get-MarketCoreExe",
            "Start-DockerDeps",
            "Update-SessionPath",
            "Find-ToolOnDisk",
            "Resolve-Tool",
            "Kitware.CMake",
        ],
        "common.ps1",
    )
    errs += must_not_match(
        bat + "\n" + ps1,
        [
            r"(?m)^\s*set\s+OPERATING_MODE=LIVE\b",
            r"(?m)^\s*set\s+LIVE_TRADING_ENABLED=true\b",
            r"--mode\s+LIVE\b",
            r"--mode\s+SHADOW\b",
            r"\$env:OPERATING_MODE\s*=\s*'LIVE'",
            r"\$env:LIVE_TRADING_ENABLED\s*=\s*'true'",
        ],
        "Install flow",
    )
    return errs


def test_v2_flow() -> list[str]:
    bat = read("V2.bat")
    ps1 = read("scripts/windows/start-v2.ps1")
    errs: list[str] = []
    errs += must_contain(
        bat,
        [
            "OPERATING_MODE=PAPER",
            "LIVE_TRADING_ENABLED=false",
            r"scripts\windows\start-v2.ps1",
        ],
        "V2.bat",
    )
    errs += must_contain(
        ps1,
        [
            "Enforce-PaperFailClosed",
            "Assert-PaperFailClosed",
            "--mode PAPER",
            "@vs-v2/control-api",
            "@vs-v2/dashboard",
            "Resolve-DashboardUrl",
            "Invoke-PaperPreflight",
            "DryRun",
            "NoBrowser",
            "Safety abort: refused non-PAPER",
            "Start-DockerDeps",
        ],
        "start-v2.ps1",
    )
    # Daily launcher must not reinstall
    errs += must_not_match(
        bat + "\n" + ps1,
        [
            r"(?m)^\s*set\s+OPERATING_MODE=LIVE\b",
            r"(?m)^\s*set\s+LIVE_TRADING_ENABLED=true\b",
            r"--mode\s+LIVE\b",
            r"--mode\s+SHADOW\b",
            r"\bnpm install\b",
        ],
        "V2 flow",
    )
    return errs


def simulate_install_dry_run() -> list[str]:
    errs: list[str] = []
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        (root / "apps" / "control-api").mkdir(parents=True)
        (root / "apps" / "dashboard").mkdir(parents=True)
        (root / "apps" / "market-core").mkdir(parents=True)
        (root / "package.json").write_text('{"name":"vs-v2"}', encoding="utf-8")
        (root / "apps" / "control-api" / "package.json").write_text("{}", encoding="utf-8")
        (root / "apps" / "dashboard" / "package.json").write_text("{}", encoding="utf-8")

        # Mirror Enforce-PaperFailClosed even if .env had LIVE
        env = {"OPERATING_MODE": "LIVE", "LIVE_TRADING_ENABLED": "true"}
        env["OPERATING_MODE"] = "PAPER"
        env["LIVE_TRADING_ENABLED"] = "false"
        if env["OPERATING_MODE"] != "PAPER" or env["LIVE_TRADING_ENABLED"] != "false":
            errs.append("simulate install: fail-closed force failed")

        marker = root / ".vs-v2-installed"
        marker.write_text(
            "installed_at=test\noperating_mode=PAPER\nlive_trading_enabled=false\n",
            encoding="utf-8",
        )
        text = marker.read_text(encoding="utf-8")
        if "operating_mode=PAPER" not in text or "live_trading_enabled=false" not in text:
            errs.append("simulate install: bad marker contents")

        # Install must not launch services / LIVE
        install_ps1 = read("scripts/windows/install.ps1")
        if re.search(r"Start-LoggedProcess|Start-Process.*market-core", install_ps1):
            errs.append("simulate install: install.ps1 starts market-core process")
        if re.search(r"--mode\s+LIVE", install_ps1):
            errs.append("simulate install: install.ps1 contains --mode LIVE")
    return errs


def simulate_v2_dry_run() -> list[str]:
    errs: list[str] = []
    planned = [
        "control-api start/dev workspace=@vs-v2/control-api",
        "market-core --mode PAPER",
        "dashboard dev workspace=@vs-v2/dashboard",
        "open browser http://127.0.0.1:5173",
    ]
    for p in planned:
        if re.search(r"--mode\s+(LIVE|SHADOW)", p, flags=re.I):
            errs.append(f"simulate V2: planned LIVE/SHADOW: {p}")
        if "npm install" in p:
            errs.append(f"simulate V2: planned reinstall: {p}")

    if not any(x.endswith("--mode PAPER") for x in planned):
        errs.append("simulate V2: market-core PAPER start missing")
    if not any("control-api" in x for x in planned):
        errs.append("simulate V2: control-api start missing")
    if not any("dashboard" in x for x in planned):
        errs.append("simulate V2: dashboard start missing")

    # Same refusal regex as start-v2.ps1
    if not re.search(r"(?i)--mode\s+(LIVE|SHADOW)", "--mode LIVE"):
        errs.append("simulate V2: refusal regex broken")
    if re.search(r"(?i)--mode\s+(LIVE|SHADOW)", "--mode PAPER"):
        errs.append("simulate V2: refusal regex false-positive on PAPER")

    # Script itself must refuse LIVE args
    ps1 = read("scripts/windows/start-v2.ps1")
    if "refused non-PAPER market-core mode" not in ps1:
        errs.append("simulate V2: missing non-PAPER refusal message")
    return errs


def test_no_live_order_paths() -> list[str]:
    errs: list[str] = []
    blob = "\n".join(
        read(p)
        for p in (
            "Install.bat",
            "V2.bat",
            "scripts/windows/common.ps1",
            "scripts/windows/install.ps1",
            "scripts/windows/start-v2.ps1",
        )
    )
    for pat in (
        r"create_position",
        r"place_order",
        r"execution-service",
        r"--bridge\b",
        r"--mode\s+LIVE",
    ):
        if re.search(pat, blob, flags=re.I):
            errs.append(f"launcher blob matches dangerous pattern {pat!r}")
    return errs


def main() -> int:
    os.environ["OPERATING_MODE"] = "PAPER"
    os.environ["LIVE_TRADING_ENABLED"] = "false"

    suites = [
        ("files", test_files_exist),
        ("install_flow", test_install_flow),
        ("v2_flow", test_v2_flow),
        ("simulate_install", simulate_install_dry_run),
        ("simulate_v2", simulate_v2_dry_run),
        ("no_live_orders", test_no_live_order_paths),
    ]
    failed = 0
    for name, fn in suites:
        errs = fn()
        if errs:
            failed += 1
            print(f"FAIL {name}")
            for e in errs:
                print(f"  - {e}")
        else:
            print(f"OK   {name}")

    if failed:
        print(f"\n{failed} suite(s) failed")
        return 1
    print("\nAll Windows startup flow checks passed (PAPER fail-closed, no LIVE orders)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
