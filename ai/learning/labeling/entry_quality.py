from __future__ import annotations

from typing import Any

from .mfe import max_favorable_excursion
from .mae import max_adverse_excursion


def entry_quality(entry: float, path: list[float], side: str = "BUY") -> float:
    mfe = max_favorable_excursion(entry, path, side)
    mae = max_adverse_excursion(entry, path, side)
    return mfe / (mae + 1e-9)


def entry_quality_from_outcome(outcome: dict[str, Any]) -> dict[str, float]:
    mfe = float(outcome.get("mfe") or 0.0)
    mae = float(outcome.get("mae") or 0.0)
    pnl = float(outcome.get("realized_pnl") or 0.0)
    quality = mfe / (mae + 1e-9)
    # Normalize soft score into [0, 1] for calibration targets.
    soft = quality / (1.0 + quality)
    return {
        "entry_quality": float(quality),
        "entry_quality_soft": float(soft),
        "entry_edge": float(pnl),
    }
