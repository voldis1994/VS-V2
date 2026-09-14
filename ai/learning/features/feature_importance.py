from __future__ import annotations

def rank_importance(importances: dict[str, float]) -> list[tuple[str, float]]:
    return sorted(importances.items(), key=lambda kv: kv[1], reverse=True)
