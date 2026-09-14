from __future__ import annotations
from dataclasses import dataclass

@dataclass
class TrainingConfig:
    model_name: str = "baseline"
    seed: int = 42
    max_epochs: int = 50
    learning_rate: float = 1e-3
