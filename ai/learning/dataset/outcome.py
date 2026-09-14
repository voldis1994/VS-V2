from __future__ import annotations
from dataclasses import dataclass

@dataclass
class Outcome:
    mfe: float = 0.0
    mae: float = 0.0
    realized_return: float = 0.0
    duration_s: float = 0.0
