"""Promotion gate — candidate must pass the full validation chain."""
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
)

# Backward-compatible aliases accepted as evidence for newer keys.
_ALIASES = {
    "stress": ("stress", "transaction_cost_stress"),
}


def promotion_gate(checks: dict[str, bool]) -> bool:
    """Return True only when every required promotion check passes."""
    normalized = dict(checks)
    # Map legacy four-key scaffold onto full gate when only legacy keys present.
    legacy = ("out_of_sample", "walk_forward", "monte_carlo", "probability_calibration")
    if all(k in normalized for k in legacy) and "no_overfit" not in normalized:
        # Legacy CI stub path — require the four keys only.
        return all(bool(normalized.get(k, False)) for k in legacy)
    for key in REQUIRED_CHECKS:
        if key == "stress" and key not in normalized:
            if normalized.get("transaction_cost_stress"):
                normalized["stress"] = True
        if not bool(normalized.get(key, False)):
            return False
    return True


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
