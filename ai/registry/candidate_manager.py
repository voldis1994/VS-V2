from __future__ import annotations
from .model_registry import ModelRegistry

class CandidateManager:
    def __init__(self, registry: ModelRegistry):
        self.registry = registry

    def list_candidates(self):
        return [m for m in self.registry.list_models() if getattr(m, "status", "") == "candidate"]
