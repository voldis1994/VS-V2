"""VS-V2 validation: walk_forward"""
from __future__ import annotations

def validate_walk_forward(folds: list[dict], min_positive: int = 1) -> bool:
    return sum(1 for f in folds if f.get('pnl', 0) > 0) >= min_positive
