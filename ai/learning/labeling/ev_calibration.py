"""EV / probability calibration labels from episode outcomes."""
from __future__ import annotations

from typing import Any


def ev_calibration_labels(features: dict[str, Any], outcome: dict[str, Any]) -> dict[str, float]:
    pred_ev = float(features.get("pred_expected_value") or 0.0)
    pred_p = float(features.get("pred_probability") or 0.0)
    pnl = float(outcome.get("realized_pnl") or 0.0)
    win = 1.0 if pnl > 0 else 0.0
    # Signed EV residual: positive means prediction understated realized edge.
    ev_error = pnl - pred_ev
    return {
        "ev_error": ev_error,
        "prob_error": win - pred_p,
        "calibrated_win": win,
    }
