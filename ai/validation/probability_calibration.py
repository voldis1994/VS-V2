"""Probability calibration check."""
from __future__ import annotations

from typing import Any

from ai.learning.evaluation.metrics import expected_calibration_error


def validate_probability_calibration(
    probs: list[float],
    outcomes: list[int],
    max_ece: float = 0.35,
) -> dict[str, Any]:
    if not probs:
        return {"passed": False, "ece": 1.0, "max_ece": max_ece}
    ece = expected_calibration_error(probs, outcomes)
    return {"passed": ece <= max_ece, "ece": ece, "max_ece": max_ece}
