from __future__ import annotations

def select_features(vectors: list[dict[str, float]], keys: list[str] | None = None) -> list[list[float]]:
    if not vectors:
        return []
    keys = keys or sorted(vectors[0].keys())
    return [[float(v.get(k, 0.0)) for k in keys] for v in vectors]
