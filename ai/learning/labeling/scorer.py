
"""Labeling scorer — adapted from decision/scorer.py."""
from __future__ import annotations
from dataclasses import dataclass

@dataclass(frozen=True)
class LabelScores:
    buy_score: float
    sell_score: float
    preferred_side: str

def label_from_scores(buy_score: float, sell_score: float, context_quality: float = 1.0) -> LabelScores:
    buy = buy_score * context_quality
    sell = sell_score * context_quality
    if buy > sell and buy > 0.55:
        side = "BUY"
    elif sell > buy and sell > 0.55:
        side = "SELL"
    else:
        side = "WAIT"
    return LabelScores(buy, sell, side)
