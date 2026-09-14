"""Guard: LIVE / production weights are never mutated by training."""
from __future__ import annotations

from pathlib import Path


class LiveMutationError(RuntimeError):
    """Raised when training attempts to mutate LIVE/production in place."""


def assert_not_live_mutation(models_root: Path, *, target: str = "candidate") -> None:
    if target != "candidate":
        raise LiveMutationError(
            f"training may only write candidates (got target={target!r})"
        )
    # Path sanity: never allow models_root itself to be production.
    name = models_root.name
    if name in {"production", "live"}:
        raise LiveMutationError("models_root must not be the production/live directory")


def assert_artifact_not_live(artifact: dict) -> None:
    status = str(artifact.get("status") or "")
    if status.lower() in {"live", "production"} and artifact.get("_mutating_in_place"):
        raise LiveMutationError("LIVE model must not change itself")
