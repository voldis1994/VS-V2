"""Market observation record for datasets."""
from __future__ import annotations
from dataclasses import dataclass, field
from typing import Any

@dataclass
class Observation:
    instrument: str
    timestamp_ns: int
    features: dict[str, float] = field(default_factory=dict)
    meta: dict[str, Any] = field(default_factory=dict)
