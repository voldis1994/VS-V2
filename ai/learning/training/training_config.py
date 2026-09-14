from __future__ import annotations

from dataclasses import dataclass


@dataclass
class TrainingConfig:
    model_name: str = "vs-v2-candidate"
    seed: int = 42
    train_ratio: float = 0.6
    oos_ratio: float = 0.2
    overfit_gap_max: float = 0.15
    min_oos_edge: float = -0.05
    learning_rate: float = 0.25
    max_scale_delta: float = 0.5
    max_epochs: int = 50  # retained for scaffold compatibility
