"""Production Stage-7 EpisodeReplay evaluation for Stage-8 candidates.

Python may train/calibrate candidates, but promotion truth comes from the
production C++ Replay/Brain path (candidate-replay-eval), not proxy_pnl.
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

from ai.episodes.loader import EPISODE_SUFFIX


@dataclass(frozen=True)
class ReplayMetrics:
    edge: float
    mean_pnl: float
    win_rate: float
    n: int
    market_events: int = 0
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


def _clamp01(x: float) -> float:
    return max(0.0, min(1.0, x))


def production_replay_episode_edge(episode: dict[str, Any], weights: dict[str, Any]) -> float:
    """
    Mirror of C++ production_replay_episode_edge coefficients.

    Fallback when the C++ binary is unavailable locally. CI builds/runs
    candidate-replay-eval (EpisodeReplay). This is NOT Python proxy_pnl.
    """
    outcome = episode.get("outcome") or {}
    frames = episode.get("frames") or []
    frame = None
    for f in frames:
        if f.get("has_decision") and f.get("has_prediction"):
            frame = f
            break
    if frame is None and frames:
        frame = frames[0]

    pred_p = 0.5
    pred_c = 0.5
    pred_r = 0.5
    if frame is not None:
        prediction = frame.get("prediction") or {}
        direction = int(outcome.get("direction") or 0)
        side = prediction.get("short_side") if direction == 2 else prediction.get("long_side")
        side = side or {}
        pred_p = float(side.get("probability") or 0.5)
        pred_c = float(side.get("continuation") or 0.5)
        pred_r = float(side.get("reversal_failure") or 0.5)

    realized = float(outcome.get("realized_pnl") or 0.0)
    mfe = float(outcome.get("mfe") or 0.0)
    mae = float(outcome.get("mae") or 0.0)
    exit_q = _clamp01(realized / mfe) if mfe > 1e-12 else 0.5

    pred_w = weights.get("prediction") or {}
    dec_w = weights.get("decision") or {}
    pos_w = weights.get("position") or {}
    p_scale = float(pred_w.get("probability_scale") or 1.0)
    c_scale = float(pred_w.get("continuation_scale") or 1.0)
    r_scale = float(pred_w.get("reversal_scale") or 1.0)
    edge_scale = float(dec_w.get("edge_scale") or 0.35)
    exit_scale = float(pos_w.get("exit_scale") or 1.0)

    cal_p = _clamp01(pred_p * p_scale)
    cal_c = _clamp01(pred_c * c_scale)
    cal_r = _clamp01(pred_r * r_scale)
    win = 1.0 if realized > 0.0 else 0.0
    align = 1.0 - abs(cal_p - win)
    cont_target = mfe / (mfe + mae + 1e-12)
    cont_term = 1.0 - abs(cal_c - cont_target)
    rev_penalty = cal_r * (1.0 - win)
    exit_term = _clamp01(exit_q * exit_scale)

    edge = realized * (0.35 + 0.35 * align + 0.15 * cont_term + 0.15 * exit_term - 0.20 * rev_penalty)
    edge *= 0.55 + 0.45 * max(0.1, edge_scale)
    if exit_scale > 1.0:
        edge *= 1.0 / exit_scale
    return edge


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
                (ep_dir / f"{eid}{EPISODE_SUFFIX}").write_text(
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
            used_production_replay=bool(payload.get("used_production_replay", True)),
            source=str(payload.get("source") or "stage7_episode_replay"),
        )


class ProductionFormulaBackend:
    """Dev-only coefficient mirror of C++ production_replay_episode_edge.

    Does NOT count as production replay for promotion (`used_production_replay=False`).
    CI must build/run candidate-replay-eval; inject FixedReplayBackend in unit tests.
    """

    def evaluate(
        self,
        episodes: list[dict[str, Any]],
        weights: dict[str, Any],
        *,
        equity: float = 50_000.0,
    ) -> ReplayMetrics:
        _ = equity
        if not episodes:
            return ReplayMetrics(
                0.0,
                0.0,
                0.0,
                0,
                used_production_replay=False,
                source="production_formula_fallback",
            )
        edges = [production_replay_episode_edge(ep, weights) for ep in episodes]
        mean = sum(edges) / len(edges)
        wins = sum(1 for e in edges if e > 0.0)
        return ReplayMetrics(
            edge=mean,
            mean_pnl=mean,
            win_rate=wins / len(edges),
            n=len(edges),
            market_events=sum(len(ep.get("market") or []) for ep in episodes),
            used_production_replay=False,
            source="production_formula_fallback",
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
        return ProductionFormulaBackend()


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
        "used_production_replay": m.used_production_replay,
        "source": m.source,
    }
