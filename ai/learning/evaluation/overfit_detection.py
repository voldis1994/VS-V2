"""Overfit detection for candidate rejection."""
from __future__ import annotations

from typing import Any


def detect_overfit(
    train_score: float, test_score: float, threshold: float = 0.15
) -> bool:
    """True when train substantially outperforms test (overfit)."""
    return (train_score - test_score) > threshold


def overfit_report(
    train_metrics: dict[str, Any],
    test_metrics: dict[str, Any],
    *,
    threshold: float = 0.15,
) -> dict[str, Any]:
    train_edge = float(train_metrics.get("edge") or 0.0)
    test_edge = float(test_metrics.get("edge") or 0.0)
    gap = train_edge - test_edge
    is_overfit = gap > threshold
    return {
        "train_edge": train_edge,
        "test_edge": test_edge,
        "gap": gap,
        "threshold": threshold,
        "overfit": is_overfit,
        "reject": is_overfit,
    }
