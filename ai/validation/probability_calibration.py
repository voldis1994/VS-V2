"""VS-V2 validation: probability_calibration"""
from __future__ import annotations

from ai.learning.evaluation.metrics import expected_calibration_error
def validate_probability_calibration(probs: list[float], outcomes: list[int], max_ece: float = 0.12) -> bool:
    return expected_calibration_error(probs, outcomes) <= max_ece
