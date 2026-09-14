from __future__ import annotations

def detect_overfit(train_score: float, test_score: float, threshold: float = 0.15) -> bool:
    return (train_score - test_score) > threshold
