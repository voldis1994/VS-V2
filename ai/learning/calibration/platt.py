
from __future__ import annotations
import math

def platt_scale(raw_score: float, a: float = 1.0, b: float = 0.0) -> float:
    z = a * raw_score + b
    return 1.0 / (1.0 + math.exp(-z))
