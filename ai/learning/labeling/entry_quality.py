from __future__ import annotations
from .mfe import max_favorable_excursion
from .mae import max_adverse_excursion

def entry_quality(entry: float, path: list[float], side: str = "BUY") -> float:
    mfe = max_favorable_excursion(entry, path, side)
    mae = max_adverse_excursion(entry, path, side)
    return mfe / (mae + 1e-9)
