"""VS-V2 validation: out_of_sample"""
from __future__ import annotations

def validate_out_of_sample(train_metrics: dict, test_metrics: dict, min_edge: float = 0.0) -> bool:
    return test_metrics.get('edge', 0) >= min_edge and test_metrics.get('edge', 0) <= train_metrics.get('edge', 1) + 0.15
