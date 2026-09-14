"""VS-V2 validation: latency_stress"""
from __future__ import annotations

def validate_latency_stress(p95_ms: float, max_ms: float = 250.0) -> bool:
    return p95_ms <= max_ms
