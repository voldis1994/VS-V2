"""Pytest defaults: prefer C++ candidate-replay-eval when built."""
from __future__ import annotations

import os
from pathlib import Path

import pytest

from ai.validation.production_replay import (
    CppCandidateReplayBackend,
    ProductionFormulaBackend,
    set_production_replay_backend,
)


@pytest.fixture(autouse=True)
def _production_replay_backend():
    """Use C++ EpisodeReplay evaluator when available; else formula fallback."""
    bin_path = Path("build/apps/market-core/candidate-replay-eval")
    if bin_path.exists():
        os.environ.setdefault("CANDIDATE_REPLAY_EVAL_BIN", str(bin_path.resolve()))
        set_production_replay_backend(CppCandidateReplayBackend(bin_path))
    else:
        set_production_replay_backend(ProductionFormulaBackend())
    yield
    set_production_replay_backend(None)
