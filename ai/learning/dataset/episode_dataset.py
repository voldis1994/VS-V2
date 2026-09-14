"""Build labeled learning rows from Stage-7 TradeEpisode JSON."""
from __future__ import annotations

from typing import Any

from ai.learning.labeling.entry_quality import entry_quality_from_outcome
from ai.learning.labeling.exit_quality import exit_quality_from_outcome
from ai.learning.labeling.continuation import continuation_reversal_labels
from ai.learning.labeling.ev_calibration import ev_calibration_labels
from ai.learning.labeling.position_quality import position_action_quality


def episode_entry_ts(ep: dict[str, Any]) -> int:
    return int((ep.get("outcome") or {}).get("entry_ts") or 0)


def episode_exit_ts(ep: dict[str, Any]) -> int:
    return int((ep.get("outcome") or {}).get("exit_ts") or 0)


def _entry_frame(ep: dict[str, Any]) -> dict[str, Any] | None:
    frames = ep.get("frames") or []
    for frame in frames:
        if frame.get("has_decision") and frame.get("has_prediction"):
            return frame
    return frames[0] if frames else None


def _side_for_direction(prediction: dict[str, Any], direction: int) -> dict[str, Any]:
    # Direction: Flat=0, Long=1, Short=2 (from C++ enums used in episodes)
    if direction == 2:
        return prediction.get("short_side") or {}
    return prediction.get("long_side") or {}


def build_sample(ep: dict[str, Any]) -> dict[str, Any]:
    """One learning sample per sealed trade episode (no future leakage into features)."""
    outcome = ep.get("outcome") or {}
    frame = _entry_frame(ep) or {}
    prediction = frame.get("prediction") or {}
    decision = frame.get("decision") or {}
    direction = int(outcome.get("direction") or decision.get("direction") or 0)
    side = _side_for_direction(prediction, direction)

    # Features: only entry-time / pre-outcome brain signals (never post_exit_*).
    features = {
        "pred_probability": float(side.get("probability") or decision.get("probability") or 0.0),
        "pred_confidence": float(side.get("confidence") or 0.0),
        "pred_expected_value": float(side.get("expected_value") or decision.get("expected_value") or 0.0),
        "pred_continuation": float(side.get("continuation") or 0.0),
        "pred_reversal_failure": float(side.get("reversal_failure") or 0.0),
        "pred_expected_move": float(side.get("expected_move") or 0.0),
        "pred_adverse_move": float(side.get("adverse_move") or 0.0),
        "pred_thesis_quality": float(side.get("thesis_quality") or 0.0),
        "pred_uncertainty": float(side.get("uncertainty") or 0.0),
        "decision_probability": float(decision.get("probability") or 0.0),
        "decision_expected_value": float(decision.get("expected_value") or 0.0),
        "decision_spread_cost": float(decision.get("spread_cost") or 0.0),
        "stop_distance_frac": float(decision.get("stop_distance_frac") or 0.0),
        "target_distance_frac": float(decision.get("target_distance_frac") or 0.0),
        "decision_action": int(frame.get("decision_action") or decision.get("action") or 0),
        "structure_volatility": float(prediction.get("structure_volatility") or 0.0),
        "structure_invalidation": float(prediction.get("structure_invalidation") or 0.0),
        "has_structure_authority": 1.0 if frame.get("has_structure_authority") else 0.0,
        "has_micro_authority": 1.0 if frame.get("has_micro_authority") else 0.0,
    }

    labels = {
        **ev_calibration_labels(features, outcome),
        **entry_quality_from_outcome(outcome),
        **continuation_reversal_labels(features, outcome),
        **exit_quality_from_outcome(outcome),
        **position_action_quality(ep),
        "realized_pnl": float(outcome.get("realized_pnl") or 0.0),
        "win": 1.0 if float(outcome.get("realized_pnl") or 0.0) > 0 else 0.0,
        "mfe": float(outcome.get("mfe") or 0.0),
        "mae": float(outcome.get("mae") or 0.0),
        "peak_retention": float(outcome.get("peak_retention") or 0.0),
        "exit_action": int(outcome.get("exit_action") or 0),
        "exit_reason": int(outcome.get("exit_reason") or 0),
        # Kept for evaluation only — never used as training features.
        "post_exit_favorable": float(outcome.get("post_exit_favorable") or 0.0),
        "post_exit_adverse": float(outcome.get("post_exit_adverse") or 0.0),
    }

    provenance = ep.get("provenance") or {}
    return {
        "episode_id": ep.get("episode_id"),
        "instrument": ep.get("instrument"),
        "entry_ts": episode_entry_ts(ep),
        "exit_ts": episode_exit_ts(ep),
        "features": features,
        "labels": labels,
        "provenance": {
            "brain_version": provenance.get("brain_version"),
            "model_id": provenance.get("model_id"),
            "config_hash": provenance.get("config_hash"),
            "episode_schema_version": provenance.get("episode_schema_version"),
        },
        "pattern_key": _pattern_key(features, labels),
    }


def _pattern_key(features: dict[str, Any], labels: dict[str, Any]) -> str:
    cont = "cont" if float(features.get("pred_continuation") or 0) >= 0.5 else "fade"
    win = "win" if labels.get("win") else "loss"
    exit_a = int(labels.get("exit_action") or 0)
    return f"{cont}|exit{exit_a}|{win}"


def build_dataset(episodes: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [build_sample(ep) for ep in episodes]
