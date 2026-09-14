from __future__ import annotations
import numpy as np

def future_path(closes: list[float], horizon: int = 12) -> list[float]:
    if len(closes) <= horizon:
        return []
    return [closes[i + horizon] / closes[i] - 1.0 for i in range(len(closes) - horizon)]
