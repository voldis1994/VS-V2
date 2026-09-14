from __future__ import annotations

def brier_score(probs: list[float], labels: list[int]) -> float:
    n = min(len(probs), len(labels))
    if n == 0:
        return 0.0
    return sum((probs[i] - labels[i]) ** 2 for i in range(n)) / n
