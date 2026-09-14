"""Deterministic Monte Carlo ruin stress (seeded)."""
from __future__ import annotations

import random
from typing import Any


def validate_monte_carlo(
    returns: list[float],
    simulations: int = 500,
    ruin_threshold: float = -0.2,
    *,
    seed: int = 42,
) -> dict[str, Any]:
    if not returns:
        return {"ruin_rate": 1.0, "passed": False}
    rng = random.Random(seed)
    ruins = 0
    for _ in range(simulations):
        equity = 0.0
        for _ in range(len(returns)):
            equity += rng.choice(returns)
            if equity <= ruin_threshold:
                ruins += 1
                break
    rate = ruins / simulations
    return {"ruin_rate": rate, "passed": rate < 0.25, "seed": seed}
