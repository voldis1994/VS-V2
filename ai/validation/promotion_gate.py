"""VS-V2 validation: promotion_gate"""
from __future__ import annotations

def promotion_gate(checks: dict[str, bool]) -> bool:
    required = ['out_of_sample', 'walk_forward', 'monte_carlo', 'probability_calibration']
    return all(checks.get(k, False) for k in required)
