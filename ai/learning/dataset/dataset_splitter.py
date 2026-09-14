from __future__ import annotations

from typing import Sequence, TypeVar

from ai.learning.dataset.splits import time_ordered_split

T = TypeVar("T")


def split_dataset(items: Sequence[T], train_ratio: float = 0.7, val_ratio: float = 0.15):
    """Legacy positional split — prefer time_ordered_split for episodes."""
    n = len(items)
    t = int(n * train_ratio)
    v = int(n * (train_ratio + val_ratio))
    return list(items[:t]), list(items[t:v]), list(items[v:])


# Re-export Stage-8 time-safe splitter
__all__ = ["split_dataset", "time_ordered_split"]
