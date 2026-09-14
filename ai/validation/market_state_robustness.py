"""VS-V2 validation: market_state_robustness"""
from __future__ import annotations

def validate_market_state_robustness(by_regime: dict[str, float], min_regimes: int = 2) -> bool:
    positive = [v for v in by_regime.values() if v > 0]
    return len(positive) >= min_regimes
