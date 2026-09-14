"""Calibratable Stage 3–6 weight space + immutable risk safety limits."""
from __future__ import annotations

import copy
import hashlib
import json
from typing import Any


# Risk safety limits — AI must NEVER mutate these (Stage-8 hard rule).
RISK_SAFETY_LIMITS: dict[str, float] = {
    "risk_budget_frac": 0.01,
    "max_position_notional_frac": 0.25,
    "max_quantity": 100.0,
    "max_gross_exposure_frac": 1.0,
    "max_net_exposure_frac": 0.5,
    "max_open_positions": 5.0,
    "max_daily_loss_frac": 0.05,
    "max_spread_frac": 0.002,
    "max_quote_age_ms": 2000.0,
    "duplicate_window_ms": 2000.0,
}

# Soft risk scales only (not safety limits) — optional calibration.
RISK_SOFT_DEFAULTS: dict[str, float] = {
    "spread_cost_scale": 1.0,
    "min_net_ev_scale": 1.0,
}

PREDICTION_DEFAULTS: dict[str, float] = {
    "adverse_base_weight": 1.0,
    "adverse_reject_weight": 1.0,
    "adverse_scale": 1.0,
    "adverse_stress_weight": 1.0,
    "align_pressure_weight": 1.0,
    "align_soft_scale": 0.002,
    "confidence_scale": 1.0,
    "continuation_scale": 1.0,
    "failed_breakout_mismatch": 0.5,
    "insufficient_confidence_scale": 0.25,
    "inv_exhaustion_weight": 1.0,
    "inv_reversal_weight": 1.0,
    "inv_structure_weight": 1.0,
    "legacy_duration_base_s": 30.0,
    "legacy_duration_span_s": 60.0,
    "legacy_momentum_scale": 0.001,
    "legacy_pressure_long": 0.6,
    "legacy_pressure_short": 0.4,
    "momentum_soft_scale": 0.002,
    "move_base_weight": 1.0,
    "move_expansion_weight": 1.0,
    "move_scale": 1.0,
    "move_vol_weight": 1.0,
    "probability_scale": 1.0,
    "reversal_scale": 1.0,
    "swing_couple_align": 1.0,
    "swing_couple_base": 1.0,
    "uncertainty_confidence_weight": 1.0,
    "w_breakout": 0.8,
    "w_compression": 0.4,
    "w_continuation": 1.0,
    "w_dynamics": 1.0,
    "w_expansion": 0.6,
    "w_failed_breakout": 1.0,
    "w_invalidation": 1.0,
    "w_micro_acceptance": 0.6,
    "w_micro_continuation": 1.0,
    "w_micro_exhaustion": 1.0,
    "w_micro_failed_breakout": 0.9,
    "w_micro_momentum": 1.0,
    "w_micro_pressure": 0.8,
    "w_micro_reclaim": 0.5,
    "w_micro_rejection": 0.8,
    "w_micro_timing": 0.7,
    "w_pullback": 0.7,
    "w_reversal": 1.0,
    "w_structure_quality": 0.8,
    "w_trend": 1.0,
}

DECISION_DEFAULTS: dict[str, float] = {
    "edge_scale": 0.35,
    "conflict_scale": 0.5,
    "weakness_scale": 0.5,
    "cost_scale": 1.0,
    "w_quality": 1.0,
    "w_continuation": 1.0,
    "weak_quality_weight": 1.0,
    "weak_ev_weight": 1.0,
    "stop_adverse_weight": 1.0,
    "stop_vol_weight": 1.0,
    "stop_invalidation_weight": 1.0,
    "stop_distance_scale": 1.0,
    "stop_move_frac": 1.0,
    "target_expected_weight": 1.0,
    "target_vol_weight": 1.0,
    "target_distance_scale": 1.0,
    "target_move_frac": 1.0,
}

EXECUTION_DEFAULTS: dict[str, float] = {
    "max_attempts": 3.0,
    "backoff_ms": 0.0,
    "dedup_window_ms": 2000.0,
    "require_positive_quantity": 1.0,
}

POSITION_DEFAULTS: dict[str, float] = {
    "w_continuation": 1.0,
    "w_invalidation": 1.0,
    "w_reversal": 1.0,
    "w_thesis_quality": 1.0,
    "w_mfe": 1.0,
    "w_mae": 1.0,
    "w_peak_retention": 1.0,
    "continuation_scale": 1.0,
    "degradation_scale": 1.0,
    "protect_scale": 1.0,
    "reduce_scale": 1.0,
    "exit_scale": 1.0,
    "dynamics_velocity_scale": 1.0,
    "mae_scale": 1.0,
    "mfe_scale": 1.0,
    "protect_degradation_mix": 0.5,
    "reduce_degradation_base": 0.5,
    "reduce_mfe_mix": 0.5,
    "stop_tighten_frac": 1.0,
    "reduce_fraction": 0.5,
    "w_dynamics": 0.25,
}


def default_weight_bundle() -> dict[str, Any]:
    return {
        "prediction": copy.deepcopy(PREDICTION_DEFAULTS),
        "decision": copy.deepcopy(DECISION_DEFAULTS),
        "execution": copy.deepcopy(EXECUTION_DEFAULTS),
        "position": copy.deepcopy(POSITION_DEFAULTS),
        "risk_soft": copy.deepcopy(RISK_SOFT_DEFAULTS),
        "risk_safety_frozen": copy.deepcopy(RISK_SAFETY_LIMITS),
    }


def assert_risk_safety_unchanged(weights: dict[str, Any]) -> None:
    frozen = weights.get("risk_safety_frozen") or {}
    for key, expected in RISK_SAFETY_LIMITS.items():
        got = frozen.get(key)
        if got is None:
            raise ValueError(f"missing frozen risk safety limit: {key}")
        if abs(float(got) - float(expected)) > 1e-12:
            raise ValueError(
                f"risk safety limit mutated by AI (forbidden): {key}={got} != {expected}"
            )
    # Soft keys must never appear inside safety block under alias names.
    for soft in RISK_SOFT_DEFAULTS:
        if soft in RISK_SAFETY_LIMITS:
            raise AssertionError("soft/safety key collision")


def clamp(value: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, value))


def stable_hash(obj: Any) -> str:
    payload = json.dumps(obj, sort_keys=True, separators=(",", ":"), default=str)
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()
