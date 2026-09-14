from __future__ import annotations

def cluster_patterns(vectors: list[list[float]], k: int = 5) -> list[int]:
    if not vectors:
        return []
    # Lightweight hash bucketing placeholder (real clustering in training pipeline)
    return [hash(tuple(round(x, 4) for x in v)) % max(1, k) for v in vectors]
