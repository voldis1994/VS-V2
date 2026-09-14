from __future__ import annotations
from .model_registry import ModelRegistry

class ModelPromoter:
    def __init__(self, registry: ModelRegistry):
        self.registry = registry

    def promote(self, name: str, version: str) -> None:
        self.registry.promote(name, version)
