
"""Pressure features — adapted from reader engine."""
from __future__ import annotations
from dataclasses import dataclass
from typing import Sequence
from ai.learning.features.structure import Bar

def _clamp(v: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, v))

@dataclass(frozen=True)
class PressureFeatures:
    buy_pressure: float
    sell_pressure: float
    pressure_delta: float

def extract_pressure_features(bars: Sequence[Bar]) -> PressureFeatures:
    if not bars:
        return PressureFeatures(0.5, 0.5, 0.0)
    buy_vals, sell_vals = [], []
    for bar in bars:
        rng = bar.high - bar.low
        if rng <= 0:
            buy_vals.append(0.5); sell_vals.append(0.5); continue
        body = bar.close - bar.open
        factor = _clamp(abs(body) / rng, 0.0, 1.0)
        if body >= 0:
            buy = 0.5 + 0.5 * factor
        else:
            buy = 0.5 - 0.5 * factor
        buy_vals.append(_clamp(buy, 0.0, 1.0))
        sell_vals.append(1.0 - buy_vals[-1])
    buy_p = sum(buy_vals) / len(buy_vals)
    sell_p = sum(sell_vals) / len(sell_vals)
    return PressureFeatures(buy_p, sell_p, buy_p - sell_p)
