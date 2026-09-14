from __future__ import annotations

from ai.registry.model_registry import ModelRegistry


class ModelPromoter:
    def __init__(self, registry: ModelRegistry):
        self.registry = registry

    def promote(self, name: str, version: str, *, validation: dict | None = None) -> None:
        self.registry.promote(name, version, validation=validation)

    def reject(self, name: str, version: str, reason: str) -> None:
        self.registry.reject(name, version, reason)

    def rollback(self, *, to_version: str | None = None) -> dict:
        return self.registry.rollback(to_version=to_version)
