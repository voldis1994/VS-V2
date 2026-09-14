from __future__ import annotations
from typing import Sequence, TypeVar
T = TypeVar("T")

def split_dataset(items: Sequence[T], train_ratio: float = 0.7, val_ratio: float = 0.15):
    n = len(items)
    t = int(n * train_ratio)
    v = int(n * (train_ratio + val_ratio))
    return list(items[:t]), list(items[t:v]), list(items[v:])
