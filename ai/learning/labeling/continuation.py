"""Continuation vs reversal outcome labels."""
from __future__ import annotations

from typing import Any


def continuation_reversal_labels(
    features: dict[str, Any], outcome: dict[str, Any]
) -> dict[str, float]:
    mfe = float(outcome.get("mfe") or 0.0)
    mae = float(outcome.get("mae") or 0.0)
    pred_c = float(features.get("pred_continuation") or 0.0)
    pred_r = float(features.get("pred_reversal_failure") or 0.0)
    # Realized continuation: MFE dominated path; reversal: MAE dominated.
    total = mfe + mae
    realized_continuation = (mfe / total) if total > 1e-12 else 0.5
    realized_reversal = 1.0 - realized_continuation
    return {
        "realized_continuation": realized_continuation,
        "realized_reversal": realized_reversal,
        "continuation_error": realized_continuation - pred_c,
        "reversal_error": realized_reversal - pred_r,
    }
