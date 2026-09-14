"""VS-V2 validation: monte_carlo"""
from __future__ import annotations

import random
def validate_monte_carlo(returns: list[float], simulations: int = 1000, ruin_threshold: float = -0.2) -> dict:
    if not returns:
        return {'ruin_rate': 1.0, 'passed': False}
    ruins = 0
    for _ in range(simulations):
        equity = 0.0
        for _ in range(len(returns)):
            equity += random.choice(returns)
            if equity <= ruin_threshold:
                ruins += 1
                break
    rate = ruins / simulations
    return {'ruin_rate': rate, 'passed': rate < 0.05}
