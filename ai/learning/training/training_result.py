from __future__ import annotations
from dataclasses import dataclass, field

@dataclass
class TrainingResult:
    model_name: str
    metrics: dict[str, float] = field(default_factory=dict)
    artifact_path: str = ""
