from __future__ import annotations

def evaluate_pattern_quality(hits: int, misses: int) -> float:
    total = hits + misses
    return hits / total if total else 0.0
