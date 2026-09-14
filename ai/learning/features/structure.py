
"""Structure features — adapted from VS_READER_ENGINE_V2 analysis/structure."""
from __future__ import annotations
from dataclasses import dataclass
from typing import Sequence

@dataclass(frozen=True)
class Bar:
    open: float
    high: float
    low: float
    close: float

@dataclass(frozen=True)
class StructureFeatures:
    swing_high: float
    swing_low: float
    structure_bias: str
    break_of_structure: bool
    support_level: float
    resistance_level: float

def extract_structure_features(bars: Sequence[Bar]) -> StructureFeatures:
    if not bars:
        return StructureFeatures(0, 0, "NEUTRAL", False, 0, 0)
    highs = [b.high for b in bars]
    lows = [b.low for b in bars]
    closes = [b.close for b in bars]
    swing_high, swing_low = max(highs), min(lows)
    first_close, last_close = closes[0], closes[-1]
    if last_close > first_close:
        bias = "BULLISH"
    elif last_close < first_close:
        bias = "BEARISH"
    else:
        bias = "NEUTRAL"
    prior_high = max(highs[:-1]) if len(highs) > 1 else highs[0]
    prior_low = min(lows[:-1]) if len(lows) > 1 else lows[0]
    bos = last_close > prior_high or last_close < prior_low
    return StructureFeatures(swing_high, swing_low, bias, bos, swing_low, swing_high)
