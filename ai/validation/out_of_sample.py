"""Out-of-sample validation."""
from __future__ import annotations

from typing import Any


def validate_out_of_sample(
    train_metrics: dict[str, Any],
    test_metrics: dict[str, Any],
    min_edge: float = 0.0,
    max_train_oos_gap: float = 0.15,
) -> bool:
    test_edge = float(test_metrics.get("edge", 0) or 0)
    train_edge = float(train_metrics.get("edge", 0) or 0)
    if test_metrics.get("n", 0) == 0:
        return False
    if test_edge < min_edge:
        return False
    if test_edge > train_edge + max_train_oos_gap:
        # Implausible OOS > train by a wide margin — treat as leakage/suspicious.
        return False
    if (train_edge - test_edge) > max_train_oos_gap:
        return False
    return True


def out_of_sample_report(
    train_metrics: dict[str, Any],
    test_metrics: dict[str, Any],
    *,
    min_edge: float = -0.05,
    max_gap: float = 0.15,
) -> dict[str, Any]:
    passed = validate_out_of_sample(
        train_metrics, test_metrics, min_edge=min_edge, max_train_oos_gap=max_gap
    )
    return {
        "passed": passed,
        "train_edge": float(train_metrics.get("edge") or 0),
        "oos_edge": float(test_metrics.get("edge") or 0),
        "min_edge": min_edge,
        "max_gap": max_gap,
    }
