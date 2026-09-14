"""Pattern ↔ outcome relationship discovery (deterministic)."""
from __future__ import annotations

from collections import defaultdict
from typing import Any


def discover_pattern_outcomes(samples: list[dict[str, Any]]) -> dict[str, Any]:
    buckets: dict[str, list[float]] = defaultdict(list)
    for s in samples:
        key = str(s.get("pattern_key") or "unknown")
        buckets[key].append(float(s["labels"].get("realized_pnl") or 0.0))

    summary: dict[str, Any] = {}
    for key in sorted(buckets.keys()):
        pnls = buckets[key]
        n = len(pnls)
        mean_pnl = sum(pnls) / n if n else 0.0
        win_rate = sum(1 for p in pnls if p > 0) / n if n else 0.0
        summary[key] = {
            "n": n,
            "mean_pnl": mean_pnl,
            "win_rate": win_rate,
        }
    return summary
