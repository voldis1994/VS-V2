"""Validation package — explicit exports (avoid shadowing submodules)."""
from __future__ import annotations

from ai.validation.monte_carlo import validate_monte_carlo
from ai.validation.out_of_sample import out_of_sample_report
from ai.validation.pipeline import train_and_validate, validate_candidate_artifact
from ai.validation.promotion_gate import evaluate_promotion, promotion_gate
from ai.validation.walk_forward import run_walk_forward

__all__ = [
    "evaluate_promotion",
    "out_of_sample_report",
    "promotion_gate",
    "run_walk_forward",
    "train_and_validate",
    "validate_candidate_artifact",
    "validate_monte_carlo",
]
