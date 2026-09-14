
"""Momentum features — adapted from reader engine."""
from __future__ import annotations
from dataclasses import dataclass
from typing import Sequence
from ai.learning.features.structure import Bar

def _clamp(v: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, v))

@dataclass(frozen=True)
class MomentumFeatures:
    momentum_score: float
    rate_of_change: float
    trend_direction: str
    trend_strength: float

def extract_momentum_features(bars: Sequence[Bar]) -> MomentumFeatures:
    if len(bars) < 2:
        return MomentumFeatures(0.0, 0.0, "SIDEWAYS", 0.0)
    first, last = bars[0].close, bars[-1].close
    roc = (last - first) / first if first else 0.0
    score = _clamp(roc * 10.0, -1.0, 1.0)
    direction = "UP" if score > 0 else "DOWN" if score < 0 else "SIDEWAYS"
    return MomentumFeatures(score, roc, direction, abs(score))
