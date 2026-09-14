"""VS-V2 validation: transaction_cost_stress"""
from __future__ import annotations

def validate_transaction_cost_stress(edge: float, cost_bps: float = 5.0) -> bool:
    return edge > cost_bps / 10000.0
