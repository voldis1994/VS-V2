from __future__ import annotations

from typing import Any


def exit_quality(mfe: float, realized: float) -> float:
    if mfe <= 0:
        return 0.0
    return max(0.0, min(1.0, realized / mfe))


def exit_quality_from_outcome(outcome: dict[str, Any]) -> dict[str, float]:
    mfe = float(outcome.get("mfe") or 0.0)
    pnl = float(outcome.get("realized_pnl") or 0.0)
    return {"exit_quality_score": exit_quality(mfe, pnl)}
