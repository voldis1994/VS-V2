"""Deterministic candidate trainer — never mutates LIVE/production in place."""
from __future__ import annotations

import copy
import json
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from ai.learning.dataset.episode_dataset import build_dataset
from ai.learning.dataset.splits import time_ordered_split
from ai.learning.evaluation.metrics import score_samples
from ai.learning.patterns.outcome_patterns import discover_pattern_outcomes
from ai.learning.training.weight_space import (
    assert_risk_safety_unchanged,
    clamp,
    default_weight_bundle,
    stable_hash,
)
from ai.registry.live_guard import assert_not_live_mutation


@dataclass
class TrainingConfig:
    model_name: str = "vs-v2-candidate"
    seed: int = 42
    train_ratio: float = 0.6
    oos_ratio: float = 0.2
    overfit_gap_max: float = 0.15
    min_oos_edge: float = -0.05
    learning_rate: float = 0.25  # scale on mean residual → weight delta
    max_scale_delta: float = 0.5


@dataclass
class TrainingResult:
    model_id: str
    version: str
    status: str
    artifact_path: str
    config_hash: str
    dataset_hash: str
    weights_hash: str
    metrics: dict[str, Any] = field(default_factory=dict)
    rejected_reason: str | None = None


def _mean(xs: list[float]) -> float:
    return sum(xs) / len(xs) if xs else 0.0


def _calibrate_weights(train_samples: list[dict[str, Any]], cfg: TrainingConfig) -> dict[str, Any]:
    """Closed-form residual → scale updates (deterministic, seed-stable)."""
    weights = default_weight_bundle()
    # Seed only affects tie-breaking salt in hash — not stochastic sampling.
    _ = cfg.seed

    prob_err = [float(s["labels"]["prob_error"]) for s in train_samples]
    ev_err = [float(s["labels"]["ev_error"]) for s in train_samples]
    cont_err = [float(s["labels"]["continuation_error"]) for s in train_samples]
    rev_err = [float(s["labels"]["reversal_error"]) for s in train_samples]
    entry_soft = [float(s["labels"]["entry_quality_soft"]) for s in train_samples]
    exit_q = [float(s["labels"]["exit_quality"]) for s in train_samples]
    protect_q = [float(s["labels"].get("protect_quality") or 0.0) for s in train_samples]
    reduce_q = [float(s["labels"].get("reduce_quality") or 0.0) for s in train_samples]
    exit_timing = [float(s["labels"].get("exit_timing_quality") or 0.5) for s in train_samples]

    lr = cfg.learning_rate
    md = cfg.max_scale_delta

    # Prediction / EV calibration
    weights["prediction"]["probability_scale"] = clamp(
        1.0 + lr * _mean(prob_err), 1.0 - md, 1.0 + md
    )
    weights["prediction"]["confidence_scale"] = clamp(
        1.0 + 0.5 * lr * _mean(prob_err), 1.0 - md, 1.0 + md
    )
    weights["prediction"]["continuation_scale"] = clamp(
        1.0 + lr * _mean(cont_err), 1.0 - md, 1.0 + md
    )
    weights["prediction"]["reversal_scale"] = clamp(
        1.0 + lr * _mean(rev_err), 1.0 - md, 1.0 + md
    )
    weights["prediction"]["w_continuation"] = clamp(
        1.0 + 0.5 * lr * _mean(cont_err), 0.5, 1.5
    )
    weights["prediction"]["w_reversal"] = clamp(
        1.0 + 0.5 * lr * _mean(rev_err), 0.5, 1.5
    )

    # Decision / entry quality
    entry_bias = _mean(entry_soft) - 0.5
    weights["decision"]["edge_scale"] = clamp(0.35 + lr * entry_bias, 0.1, 0.8)
    weights["decision"]["w_quality"] = clamp(1.0 + lr * entry_bias, 0.5, 1.5)
    weights["decision"]["w_continuation"] = clamp(
        1.0 + 0.5 * lr * _mean(cont_err), 0.5, 1.5
    )
    weights["decision"]["cost_scale"] = clamp(
        1.0 - 0.25 * lr * _mean(ev_err), 0.5, 1.5
    )

    # PositionBrain exit/protect/reduce quality
    exit_bias = _mean(exit_q) - 0.5
    timing_bias = _mean(exit_timing) - 0.5
    weights["position"]["exit_scale"] = clamp(1.0 + lr * (exit_bias + timing_bias), 0.5, 1.5)
    weights["position"]["protect_scale"] = clamp(1.0 + lr * (_mean(protect_q) - 0.5), 0.5, 1.5)
    weights["position"]["reduce_scale"] = clamp(1.0 + lr * (_mean(reduce_q) - 0.5), 0.5, 1.5)
    weights["position"]["continuation_scale"] = clamp(
        1.0 + 0.5 * lr * _mean(cont_err), 0.5, 1.5
    )
    weights["position"]["degradation_scale"] = clamp(
        1.0 + 0.5 * lr * _mean(rev_err), 0.5, 1.5
    )
    weights["position"]["mfe_scale"] = clamp(1.0 + 0.25 * lr * exit_bias, 0.5, 1.5)
    weights["position"]["mae_scale"] = clamp(1.0 - 0.25 * lr * exit_bias, 0.5, 1.5)

    # Soft risk scales only (safety frozen untouched)
    weights["risk_soft"]["min_net_ev_scale"] = clamp(
        1.0 + 0.1 * lr * _mean(ev_err), 0.5, 1.5
    )
    weights["risk_soft"]["spread_cost_scale"] = clamp(
        1.0 + 0.05 * lr * abs(_mean(ev_err)), 0.5, 1.5
    )

    assert_risk_safety_unchanged(weights)
    return weights


def _apply_score(samples: list[dict[str, Any]], weights: dict[str, Any]) -> list[dict[str, Any]]:
    """Training-only proxy diagnostic score — NEVER used for promotion gates.

    Promotion truth is Stage-7 production EpisodeReplay (see ai.validation.production_replay).
    """
    p_scale = float(weights["prediction"]["probability_scale"])
    c_scale = float(weights["prediction"]["continuation_scale"])
    edge = float(weights["decision"]["edge_scale"])
    exit_s = float(weights["position"]["exit_scale"])
    scored: list[dict[str, Any]] = []
    for s in samples:
        f = s["features"]
        lab = s["labels"]
        cal_p = clamp(float(f["pred_probability"]) * p_scale, 0.0, 1.0)
        cal_c = clamp(float(f["pred_continuation"]) * c_scale, 0.0, 1.0)
        # Proxy PnL diagnostic only (training). Not production-replay truth.
        align = 1.0 - abs(cal_p - float(lab["win"]))
        cont_align = 1.0 - abs(cal_c - float(lab["realized_continuation"]))
        exit_align = float(lab["exit_quality"]) * exit_s
        proxy = float(lab["realized_pnl"]) * (
            0.5 + 0.25 * align + 0.15 * cont_align + 0.1 * min(1.0, exit_align)
        )
        proxy *= 0.5 + edge
        row = copy.deepcopy(s)
        row["score"] = {
            "calibrated_probability": cal_p,
            "calibrated_continuation": cal_c,
            "proxy_pnl": proxy,  # training diagnostic only
            "edge": proxy,
        }
        scored.append(row)
    return scored


def train_candidate(
    episodes: list[dict[str, Any]],
    *,
    models_root: Path,
    cfg: TrainingConfig | None = None,
    parent_production_version: str | None = None,
) -> TrainingResult:
    """
    Train a candidate model from historical episodes.

    Writes ONLY under models/candidate/. Never mutates production/LIVE.
    """
    cfg = cfg or TrainingConfig()
    assert_not_live_mutation(models_root, target="candidate")

    samples = build_dataset(episodes)
    if len(samples) < 2:
        raise ValueError("need at least 2 sealed episodes to train")

    train, oos, shadow = time_ordered_split(
        samples, train_ratio=cfg.train_ratio, oos_ratio=cfg.oos_ratio
    )
    if not train:
        raise ValueError("empty train split")

    weights = _calibrate_weights(train, cfg)
    patterns = discover_pattern_outcomes(train)

    train_scored = _apply_score(train, weights)
    oos_scored = _apply_score(oos, weights) if oos else []
    shadow_scored = _apply_score(shadow, weights) if shadow else []

    train_metrics = score_samples(train_scored)
    oos_metrics = score_samples(oos_scored) if oos_scored else {"edge": 0.0, "n": 0}
    shadow_metrics = score_samples(shadow_scored) if shadow_scored else {"edge": 0.0, "n": 0}

    dataset_hash = stable_hash(
        [{"episode_id": s["episode_id"], "entry_ts": s["entry_ts"]} for s in samples]
    )
    config_hash = stable_hash(asdict(cfg))
    weights_hash = stable_hash(weights)
    version = f"c-{weights_hash[:12]}"
    model_id = cfg.model_name

    provenance = {
        "brain_version": (samples[0].get("provenance") or {}).get("brain_version"),
        "episode_schema_version": (samples[0].get("provenance") or {}).get(
            "episode_schema_version"
        ),
        "parent_production_version": parent_production_version,
    }

    artifact: dict[str, Any] = {
        "schema_version": "vs-v2-model-1",
        "model_id": model_id,
        "version": version,
        "status": "candidate",
        "created_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "seed": cfg.seed,
        "training_config": asdict(cfg),
        "training_config_hash": config_hash,
        "dataset_hash": dataset_hash,
        "weights_hash": weights_hash,
        "provenance": provenance,
        "weights": weights,
        "patterns": patterns,
        "splits": {
            "train_ids": [s["episode_id"] for s in train],
            "oos_ids": [s["episode_id"] for s in oos],
            "shadow_ids": [s["episode_id"] for s in shadow],
        },
        "metrics": {
            "train": train_metrics,
            "oos": oos_metrics,
            "shadow": shadow_metrics,
        },
        "scored": {
            "train": [{"episode_id": s["episode_id"], **s["score"]} for s in train_scored],
            "oos": [{"episode_id": s["episode_id"], **s["score"]} for s in oos_scored],
            "shadow": [{"episode_id": s["episode_id"], **s["score"]} for s in shadow_scored],
        },
    }

    out_dir = models_root / "candidate" / model_id / version
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / "model.json"
    path.write_text(json.dumps(artifact, indent=2, sort_keys=True), encoding="utf-8")
    (out_dir / "weights.json").write_text(
        json.dumps(weights, indent=2, sort_keys=True), encoding="utf-8"
    )
    (out_dir / "manifest.json").write_text(
        json.dumps(
            {
                "model_id": model_id,
                "version": version,
                "status": "candidate",
                "config_hash": config_hash,
                "dataset_hash": dataset_hash,
                "weights_hash": weights_hash,
            },
            indent=2,
            sort_keys=True,
        ),
        encoding="utf-8",
    )

    return TrainingResult(
        model_id=model_id,
        version=version,
        status="candidate",
        artifact_path=str(path),
        config_hash=config_hash,
        dataset_hash=dataset_hash,
        weights_hash=weights_hash,
        metrics=artifact["metrics"],
    )


# Back-compat thin wrapper for older scaffold entrypoint.
def train_baseline(dataset_path: Path, model_out: Path) -> dict[str, Any]:
    data = json.loads(dataset_path.read_text()) if dataset_path.exists() else {"rows": []}
    rows = data.get("rows") or []
    # If rows look like episodes, train candidate into sibling candidate dir.
    if rows and isinstance(rows[0], dict) and "outcome" in rows[0]:
        root = model_out.parent if model_out.suffix else model_out
        # Expect models root two levels up from file path convention.
        models_root = root if root.name in {"models"} else root.parent
        result = train_candidate(rows, models_root=models_root)
        return {"samples": len(rows), "model": result.artifact_path, "version": result.version}
    weights = {"structure": 0.25, "momentum": 0.35, "pressure": 0.25, "behavior": 0.15}
    model_out.parent.mkdir(parents=True, exist_ok=True)
    model_out.write_text(
        json.dumps(
            {"type": "baseline", "weights": weights, "samples": len(rows)}, indent=2
        )
    )
    return {"samples": len(rows), "model": str(model_out)}
