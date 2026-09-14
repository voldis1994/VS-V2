from __future__ import annotations
from dataclasses import dataclass, field

@dataclass
class ModelMetadata:
    name: str
    version: str
    created_at: str = ""
    metrics: dict[str, float] = field(default_factory=dict)
    status: str = "candidate"
