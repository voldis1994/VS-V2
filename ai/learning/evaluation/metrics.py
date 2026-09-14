
from __future__ import annotations

def accuracy(y_true: list[int], y_pred: list[int]) -> float:
    if not y_true:
        return 0.0
    correct = sum(1 for t, p in zip(y_true, y_pred) if t == p)
    return correct / len(y_true)

def expected_calibration_error(probs: list[float], outcomes: list[int], bins: int = 10) -> float:
    if not probs:
        return 0.0
    bucket_size = max(1, len(probs) // bins)
    ece = 0.0
    for i in range(0, len(probs), bucket_size):
        chunk_p = probs[i:i + bucket_size]
        chunk_y = outcomes[i:i + bucket_size]
        if not chunk_p:
            continue
        conf = sum(chunk_p) / len(chunk_p)
        acc = sum(chunk_y) / len(chunk_y)
        ece += abs(conf - acc) * len(chunk_p) / len(probs)
    return ece
