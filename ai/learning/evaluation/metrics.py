"""Evaluation metrics for scored episode samples."""
from __future__ import annotations

from typing import Any


def accuracy(y_true: list[int], y_pred: list[int]) -> float:
    if not y_true:
        return 0.0
    correct = sum(1 for t, p in zip(y_true, y_pred) if t == p)
    return correct / len(y_true)


def expected_calibration_error(
    probs: list[float], outcomes: list[int], bins: int = 10
) -> float:
    if not probs:
        return 0.0
    bucket_size = max(1, len(probs) // bins)
    ece = 0.0
    for i in range(0, len(probs), bucket_size):
        chunk_p = probs[i : i + bucket_size]
        chunk_y = outcomes[i : i + bucket_size]
        if not chunk_p:
            continue
        conf = sum(chunk_p) / len(chunk_p)
        acc = sum(chunk_y) / len(chunk_y)
        ece += abs(conf - acc) * len(chunk_p) / len(probs)
    return ece


def score_samples(scored: list[dict[str, Any]]) -> dict[str, float]:
    if not scored:
        return {
            "n": 0.0,
            "edge": 0.0,
            "mean_pnl": 0.0,
            "win_rate": 0.0,
            "ece": 1.0,
            "mean_exit_quality": 0.0,
        }
    pnls = [float(s["score"]["proxy_pnl"]) for s in scored]
    wins = [1 if float(s["labels"]["win"]) > 0 else 0 for s in scored]
    probs = [float(s["score"]["calibrated_probability"]) for s in scored]
    exits = [float(s["labels"].get("exit_quality") or 0.0) for s in scored]
    mean_pnl = sum(pnls) / len(pnls)
    return {
        "n": float(len(scored)),
        "edge": mean_pnl,
        "mean_pnl": mean_pnl,
        "win_rate": sum(wins) / len(wins),
        "ece": expected_calibration_error(probs, wins),
        "mean_exit_quality": sum(exits) / len(exits),
    }
