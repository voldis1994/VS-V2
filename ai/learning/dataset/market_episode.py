"""Episode spanning a market decision lifecycle."""
from __future__ import annotations
from dataclasses import dataclass, field
from .observation import Observation

@dataclass
class MarketEpisode:
    episode_id: str
    observations: list[Observation] = field(default_factory=list)
    action: str = "WAIT"
    outcome_pnl: float = 0.0
