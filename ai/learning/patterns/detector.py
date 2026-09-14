
from __future__ import annotations
from dataclasses import dataclass
from typing import Sequence
from ai.learning.features.structure import Bar

@dataclass(frozen=True)
class PatternHit:
    name: str
    confidence: float

def detect_patterns(bars: Sequence[Bar]) -> list[PatternHit]:
    if len(bars) < 3:
        return []
    last = bars[-1]
    body = last.close - last.open
    rng = last.high - last.low
    if rng > 0 and abs(body) / rng > 0.7:
        direction = "BULLISH_MARUBOZU" if body > 0 else "BEARISH_MARUBOZU"
        return [PatternHit(direction, min(1.0, abs(body) / rng))]
    return []
