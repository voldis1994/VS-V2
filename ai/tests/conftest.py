"""Pytest defaults: prefer C++ candidate-replay-eval when built.

Never fall back to a manual scoring formula — without the binary, production
replay is unavailable (used_production_replay=False).
"""
from __future__ import annotations

import os
from pathlib import Path

import pytest

from ai.validation.production_replay import (
    CppCandidateReplayBackend,
    UnavailableReplayBackend,
    set_production_replay_backend,
)


@pytest.fixture(autouse=True)
def _production_replay_backend():
    bin_path = Path("build/apps/market-core/candidate-replay-eval")
    if bin_path.exists():
        os.environ.setdefault("CANDIDATE_REPLAY_EVAL_BIN", str(bin_path.resolve()))
        set_production_replay_backend(CppCandidateReplayBackend(bin_path))
    else:
        set_production_replay_backend(UnavailableReplayBackend())
    yield
    set_production_replay_backend(None)
