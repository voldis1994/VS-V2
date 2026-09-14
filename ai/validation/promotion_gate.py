"""Promotion gate — candidate must pass EVERY Stage-8 check (no legacy bypass)."""
from __future__ import annotations

from typing import Any


REQUIRED_CHECKS = (
    "out_of_sample",
    "walk_forward",
    "stress",
    "monte_carlo",
    "probability_calibration",
    "no_overfit",
    "no_leakage",
    "shadow_paper",
    "risk_safety_frozen",
    "reproducible",
    "production_replay",
)


def promotion_gate(checks: dict[str, bool]) -> bool:
    """Return True only when every required Stage-8 promotion check passes."""
    return all(bool(checks.get(k, False)) for k in REQUIRED_CHECKS)


def evaluate_promotion(report: dict[str, Any]) -> dict[str, Any]:
    checks = dict(report.get("checks") or {})
    missing = [k for k in REQUIRED_CHECKS if k not in checks]
    for k in missing:
        checks[k] = False
    passed = all(bool(checks[k]) for k in REQUIRED_CHECKS)
    return {
        "passed": passed,
        "checks": checks,
        "missing": missing,
        "reject_reasons": [k for k in REQUIRED_CHECKS if not checks.get(k)],
    }
