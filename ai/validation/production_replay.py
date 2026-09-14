"""Production Stage-7 EpisodeReplay evaluation for Stage-8 candidates.

Python may train/calibrate candidates, but promotion truth comes from the
production C++ Replay/Brain path (candidate-replay-eval). Metrics are the
real replay decisions/trades/PnL — never a manual score_after_replay /
proxy_pnl formula, and never the recorded episode outcome as candidate result.
"""
from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Protocol

from ai.episodes.loader import EPISODE_SUFFIX as EPISODE_FILE_SUFFIX


@dataclass(frozen=True)
class ReplayMetrics:
    edge: float
    mean_pnl: float
    win_rate: float
    n: int
    market_events: int = 0
    decisions: int = 0
    entry_ready: int = 0
    entries: int = 0
    exits: int = 0
    used_production_replay: bool = True
    source: str = "stage7_episode_replay"


class ProductionReplayBackend(Protocol):
    def evaluate(
        self,
        episodes: list[dict[str, Any]],
        weights: dict[str, Any],
        *,
        equity: float = 50_000.0,
    ) -> ReplayMetrics: ...


def _find_replay_binary() -> Path | None:
    env = os.environ.get("CANDIDATE_REPLAY_EVAL_BIN")
    if env:
        p = Path(env)
        if p.exists():
            return p
    candidates = [
        Path("build/apps/market-core/candidate-replay-eval"),
        Path("/workspace/build/apps/market-core/candidate-replay-eval"),
        Path(__file__).resolve().parents[2]
        / "build"
        / "apps"
        / "market-core"
        / "candidate-replay-eval",
    ]
    for c in candidates:
        if c.exists():
            return c
    which = shutil.which("candidate-replay-eval")
    return Path(which) if which else None


class CppCandidateReplayBackend:
    """Invokes production C++ candidate-replay-eval (EpisodeReplay + Brain)."""

    def __init__(self, binary: Path | None = None):
        self.binary = binary or _find_replay_binary()
        if self.binary is None:
            raise FileNotFoundError(
                "candidate-replay-eval binary not found; build market-core tool "
                "or set CANDIDATE_REPLAY_EVAL_BIN"
            )

    def evaluate(
        self,
        episodes: list[dict[str, Any]],
        weights: dict[str, Any],
        *,
        equity: float = 50_000.0,
    ) -> ReplayMetrics:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            ep_dir = root / "episodes"
            ep_dir.mkdir()
            for ep in episodes:
                eid = str(ep.get("episode_id") or "ep")
                (ep_dir / f"{eid}{EPISODE_FILE_SUFFIX}").write_text(
                    json.dumps(ep, sort_keys=True), encoding="utf-8"
                )
            weights_path = root / "weights.json"
            weights_path.write_text(
                json.dumps({"weights": weights}, sort_keys=True), encoding="utf-8"
            )
            proc = subprocess.run(
                [
                    str(self.binary),
                    "--episodes-dir",
                    str(ep_dir),
                    "--weights",
                    str(weights_path),
                    "--equity",
                    str(equity),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            payload = json.loads(proc.stdout)
        return ReplayMetrics(
            edge=float(payload.get("edge") or 0.0),
            mean_pnl=float(payload.get("mean_pnl") or 0.0),
            win_rate=float(payload.get("win_rate") or 0.0),
            n=int(payload.get("n") or 0),
            market_events=int(payload.get("market_events") or 0),
            decisions=int(payload.get("decisions") or 0),
            entry_ready=int(payload.get("entry_ready") or 0),
            entries=int(payload.get("entries") or 0),
            exits=int(payload.get("exits") or 0),
            used_production_replay=bool(payload.get("used_production_replay", True)),
            source=str(payload.get("source") or "stage7_episode_replay"),
        )


class UnavailableReplayBackend:
    """No C++ binary — cannot claim production replay truth (no manual formula)."""

    def evaluate(
        self,
        episodes: list[dict[str, Any]],
        weights: dict[str, Any],
        *,
        equity: float = 50_000.0,
    ) -> ReplayMetrics:
        _ = episodes, weights, equity
        return ReplayMetrics(
            edge=0.0,
            mean_pnl=0.0,
            win_rate=0.0,
            n=0,
            used_production_replay=False,
            source="production_replay_unavailable",
        )


class FixedReplayBackend:
    """Test double — inject explicit production-replay metrics."""

    def __init__(self, metrics: ReplayMetrics):
        self.metrics = metrics

    def evaluate(
        self,
        episodes: list[dict[str, Any]],
        weights: dict[str, Any],
        *,
        equity: float = 50_000.0,
    ) -> ReplayMetrics:
        _ = episodes, weights, equity
        return self.metrics


_default_backend: ProductionReplayBackend | None = None


def set_production_replay_backend(backend: ProductionReplayBackend | None) -> None:
    global _default_backend
    _default_backend = backend


def get_production_replay_backend() -> ProductionReplayBackend:
    if _default_backend is not None:
        return _default_backend
    try:
        return CppCandidateReplayBackend()
    except FileNotFoundError:
        return UnavailableReplayBackend()


def evaluate_weights_on_episodes(
    episodes: list[dict[str, Any]],
    weights: dict[str, Any],
    *,
    equity: float = 50_000.0,
    backend: ProductionReplayBackend | None = None,
) -> ReplayMetrics:
    """Run production replay evaluation for candidate weights."""
    b = backend or get_production_replay_backend()
    return b.evaluate(episodes, weights, equity=equity)


def metrics_to_dict(m: ReplayMetrics) -> dict[str, float | int | bool | str]:
    return {
        "edge": m.edge,
        "mean_pnl": m.mean_pnl,
        "win_rate": m.win_rate,
        "n": float(m.n),
        "market_events": float(m.market_events),
        "decisions": float(m.decisions),
        "entry_ready": float(m.entry_ready),
        "entries": float(m.entries),
        "exits": float(m.exits),
        "used_production_replay": m.used_production_replay,
        "source": m.source,
    }
