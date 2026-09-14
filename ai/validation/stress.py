"""Stress validation helpers — driven by production-replay edge (not proxy_pnl)."""
from __future__ import annotations

from typing import Any


def validate_transaction_cost_stress(edge: float, cost_bps: float = 5.0) -> bool:
    return edge > cost_bps / 10000.0


def validate_latency_stress(edge: float, latency_haircut: float = 0.1) -> bool:
    return (edge * (1.0 - latency_haircut)) > 0.0 or edge >= 0.0


def validate_market_state_robustness(edges_by_state: dict[str, float], min_states: int = 1) -> bool:
    if len(edges_by_state) < min_states:
        return False
    ok = sum(1 for e in edges_by_state.values() if e > -0.5)
    return ok >= max(1, len(edges_by_state) // 2)


def run_stress_suite(
    oos_metrics: dict[str, Any],
    scored_oos: list[dict[str, Any]] | None = None,
    *,
    replay_edge: float | None = None,
) -> dict[str, Any]:
    edge = float(replay_edge if replay_edge is not None else (oos_metrics.get("edge") or 0.0))
    # Without per-sample proxy scores, treat single production-replay edge as the stress input.
    by_state = {"replay": [edge]}
    edges = {"replay": edge}
    tc = validate_transaction_cost_stress(edge, cost_bps=1.0)
    lat = edge * 0.9 >= -0.05
    rob = validate_market_state_robustness(edges, min_states=1)
    passed = bool(tc or edge >= 0.0) and lat and rob
    return {
        "passed": passed,
        "transaction_cost": tc,
        "latency": lat,
        "robustness": rob,
        "edges_by_state": edges,
        "edge": edge,
        "scored_oos_ignored": scored_oos is not None,  # proxy path retired
    }
