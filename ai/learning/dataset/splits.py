"""Time-ordered dataset splits with leakage guards."""
from __future__ import annotations

from typing import Any, Sequence, TypeVar

T = TypeVar("T")


class LeakageError(ValueError):
    """Raised when a split would allow future information into training."""


def _ts(sample: dict[str, Any], field: str = "entry_ts") -> int:
    return int(sample.get(field) or 0)


def assert_no_leakage(
    train: Sequence[dict[str, Any]],
    holdout: Sequence[dict[str, Any]],
    *,
    train_time_field: str = "exit_ts",
    holdout_time_field: str = "entry_ts",
) -> None:
    """Train must end before holdout begins (exit_ts_train < entry_ts_holdout)."""
    if not train or not holdout:
        return
    train_end = max(_ts(s, train_time_field) for s in train)
    holdout_start = min(_ts(s, holdout_time_field) for s in holdout)
    if holdout_start <= train_end:
        raise LeakageError(
            f"data leakage: holdout starts at {holdout_start} <= train ends at {train_end}"
        )
    # Features must not include post-exit labels.
    for sample in list(train) + list(holdout):
        feats = sample.get("features") or {}
        for banned in ("post_exit_favorable", "post_exit_adverse", "realized_pnl", "mfe", "mae"):
            if banned in feats:
                raise LeakageError(f"label leaked into features: {banned}")


def time_ordered_split(
    samples: Sequence[dict[str, Any]],
    *,
    train_ratio: float = 0.6,
    oos_ratio: float = 0.2,
) -> tuple[list[dict[str, Any]], list[dict[str, Any]], list[dict[str, Any]]]:
    """Split sorted samples into train / oos / holdout (shadow)."""
    ordered = sorted(samples, key=lambda s: (_ts(s, "entry_ts"), s.get("episode_id", "")))
    n = len(ordered)
    if n == 0:
        return [], [], []
    t = max(1, int(n * train_ratio)) if n >= 3 else max(1, n - 1)
    o = t + max(1, int(n * oos_ratio)) if n >= 3 else n
    o = min(o, n)
    train = list(ordered[:t])
    oos = list(ordered[t:o])
    shadow = list(ordered[o:])
    if oos:
        assert_no_leakage(train, oos)
    if shadow:
        assert_no_leakage(train + oos, shadow)
    return train, oos, shadow


def walk_forward_folds(
    samples: Sequence[dict[str, Any]],
    *,
    n_folds: int = 3,
    min_train: int = 2,
) -> list[dict[str, Any]]:
    """Expanding-window walk-forward folds; each test window is strictly after train."""
    ordered = sorted(samples, key=lambda s: (_ts(s, "entry_ts"), s.get("episode_id", "")))
    n = len(ordered)
    if n < min_train + 1:
        return []
    folds: list[dict[str, Any]] = []
    # Reserve last portion for successive test slices.
    test_size = max(1, n // (n_folds + 1))
    for i in range(n_folds):
        test_end = n - i * test_size
        test_start = max(min_train, test_end - test_size)
        if test_start >= test_end:
            continue
        train = list(ordered[:test_start])
        test = list(ordered[test_start:test_end])
        if len(train) < min_train or not test:
            continue
        assert_no_leakage(train, test)
        folds.append({"fold": len(folds), "train": train, "test": test})
    folds.reverse()  # chronological fold order
    return folds
