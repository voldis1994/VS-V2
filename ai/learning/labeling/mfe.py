from __future__ import annotations

def max_favorable_excursion(entry: float, path: list[float], side: str = "BUY") -> float:
    if not path:
        return 0.0
    if side.upper() == "BUY":
        return max(0.0, max(path) - entry)
    return max(0.0, entry - min(path))
