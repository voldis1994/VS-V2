"""Walk-forward validation."""
from __future__ import annotations

from typing import Any

from ai.learning.dataset.splits import walk_forward_folds
from ai.learning.evaluation.metrics import score_samples
from ai.learning.training.trainer import TrainingConfig, _apply_score, _calibrate_weights


def validate_walk_forward(folds: list[dict], min_positive: int = 1) -> bool:
    return sum(1 for f in folds if f.get("pnl", 0) > 0) >= min_positive


def run_walk_forward(
    samples: list[dict[str, Any]],
    *,
    cfg: TrainingConfig | None = None,
    n_folds: int = 3,
    min_positive: int = 1,
) -> dict[str, Any]:
    cfg = cfg or TrainingConfig()
    raw_folds = walk_forward_folds(samples, n_folds=n_folds)
    results: list[dict[str, Any]] = []
    for fold in raw_folds:
        weights = _calibrate_weights(fold["train"], cfg)
        scored = _apply_score(fold["test"], weights)
        metrics = score_samples(scored)
        results.append(
            {
                "fold": fold["fold"],
                "pnl": metrics["edge"],
                "edge": metrics["edge"],
                "n_train": len(fold["train"]),
                "n_test": len(fold["test"]),
                "metrics": metrics,
            }
        )
    passed = validate_walk_forward(results, min_positive=min_positive) if results else False
    return {"passed": passed, "folds": results, "n_folds": len(results)}
