"""Full candidate validation: production EpisodeReplay → OOS/WF/stress → gate.

Training may use Python proxy diagnostics, but promotion truth is Stage-7
production C++ Replay/Brain outcomes only (never proxy_pnl).
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from ai.learning.dataset.episode_dataset import build_dataset
from ai.learning.dataset.splits import LeakageError, assert_no_leakage, time_ordered_split, walk_forward_folds
from ai.learning.evaluation.overfit_detection import overfit_report
from ai.learning.training.trainer import TrainingConfig, train_candidate
from ai.learning.training.weight_space import assert_risk_safety_unchanged, default_weight_bundle, stable_hash
from ai.validation.monte_carlo import validate_monte_carlo
from ai.validation.out_of_sample import out_of_sample_report
from ai.validation.probability_calibration import validate_probability_calibration
from ai.validation.production_replay import (
    ReplayMetrics,
    evaluate_weights_on_episodes,
    metrics_to_dict,
)  # production replay truth (not proxy_pnl)
from ai.validation.promotion_gate import evaluate_promotion
from ai.validation.stress import run_stress_suite


def _episodes_for_samples(
    episodes: list[dict[str, Any]], samples: list[dict[str, Any]]
) -> list[dict[str, Any]]:
    by_id = {str(e.get("episode_id")): e for e in episodes}
    out: list[dict[str, Any]] = []
    for s in samples:
        ep = by_id.get(str(s.get("episode_id")))
        if ep is not None:
            out.append(ep)
    return out


def _replay_metrics_as_gate_metrics(m: ReplayMetrics) -> dict[str, float]:
    return {
        "n": float(m.n),
        "edge": float(m.edge),
        "mean_pnl": float(m.mean_pnl),
        "win_rate": float(m.win_rate),
        # ECE not available from aggregate replay; keep neutral placeholder for calibration gate inputs.
        "ece": 0.0,
        "mean_exit_quality": 0.0,
    }


def validate_candidate_artifact(
    artifact: dict[str, Any],
    episodes: list[dict[str, Any]],
    *,
    cfg: TrainingConfig | None = None,
) -> dict[str, Any]:
    cfg = cfg or TrainingConfig(seed=int(artifact.get("seed") or 42))
    samples = build_dataset(episodes)
    train, oos, shadow = time_ordered_split(
        samples, train_ratio=cfg.train_ratio, oos_ratio=cfg.oos_ratio
    )

    checks: dict[str, bool] = {}
    details: dict[str, Any] = {}

    try:
        if oos:
            assert_no_leakage(train, oos)
        if shadow:
            assert_no_leakage(train + oos, shadow)
        checks["no_leakage"] = True
    except LeakageError as exc:
        checks["no_leakage"] = False
        details["leakage_error"] = str(exc)

    weights = artifact.get("weights") or {}
    try:
        assert_risk_safety_unchanged(weights)
        checks["risk_safety_frozen"] = True
    except ValueError as exc:
        checks["risk_safety_frozen"] = False
        details["risk_safety_error"] = str(exc)

    checks["reproducible"] = bool(artifact.get("weights_hash")) and artifact[
        "weights_hash"
    ] == stable_hash(weights)

    # --- Production Stage-7 replay evaluation (promotion truth) ---
    train_eps = _episodes_for_samples(episodes, train)
    oos_eps = _episodes_for_samples(episodes, oos)
    shadow_eps = _episodes_for_samples(episodes, shadow)

    baseline = default_weight_bundle()
    cand_train = evaluate_weights_on_episodes(train_eps, weights)
    cand_oos = evaluate_weights_on_episodes(oos_eps, weights) if oos_eps else ReplayMetrics(0, 0, 0, 0)
    cand_shadow = (
        evaluate_weights_on_episodes(shadow_eps, weights) if shadow_eps else ReplayMetrics(0, 0, 0, 0)
    )
    base_oos = evaluate_weights_on_episodes(oos_eps, baseline) if oos_eps else ReplayMetrics(0, 0, 0, 0)

    train_m = _replay_metrics_as_gate_metrics(cand_train)
    oos_m = _replay_metrics_as_gate_metrics(cand_oos) if oos_eps else {"edge": 0.0, "n": 0}
    shadow_m = _replay_metrics_as_gate_metrics(cand_shadow) if shadow_eps else {"edge": 0.0, "n": 0}

    details["production_replay"] = {
        "train": metrics_to_dict(cand_train),
        "oos": metrics_to_dict(cand_oos),
        "shadow": metrics_to_dict(cand_shadow),
        "baseline_oos": metrics_to_dict(base_oos),
    }

    # Candidate must not underperform baseline defaults on OOS production replay.
    prod_ok = bool(cand_oos.used_production_replay) and (
        (not oos_eps) or (cand_oos.edge + 1e-12 >= base_oos.edge)
    )
    checks["production_replay"] = prod_ok and cand_train.used_production_replay

    oos_rep = out_of_sample_report(train_m, oos_m, min_edge=cfg.min_oos_edge)
    checks["out_of_sample"] = bool(oos_rep["passed"]) and checks["production_replay"]
    details["out_of_sample"] = oos_rep

    # Walk-forward on production replay edges (recalibrate per fold, score test via replay).
    from ai.learning.training.trainer import _calibrate_weights

    wf_results: list[dict[str, Any]] = []
    for fold in walk_forward_folds(samples, n_folds=3):
        fold_w = _calibrate_weights(fold["train"], cfg)
        fold_eps = _episodes_for_samples(episodes, fold["test"])
        fold_m = evaluate_weights_on_episodes(fold_eps, fold_w)
        wf_results.append(
            {
                "fold": fold["fold"],
                "pnl": fold_m.edge,
                "edge": fold_m.edge,
                "n_train": len(fold["train"]),
                "n_test": len(fold["test"]),
                "production_replay": metrics_to_dict(fold_m),
            }
        )
    wf_passed = sum(1 for f in wf_results if f.get("pnl", 0) > 0) >= 1 if wf_results else False
    checks["walk_forward"] = bool(wf_passed)
    details["walk_forward"] = {"passed": wf_passed, "folds": wf_results, "n_folds": len(wf_results)}

    # Stress / MC from production replay OOS edge (not proxy_pnl).
    stress = run_stress_suite(oos_m, replay_edge=float(oos_m.get("edge") or 0.0))
    checks["stress"] = bool(stress["passed"])
    details["stress"] = stress

    returns = [float(oos_m.get("edge") or 0.0)] * max(1, int(oos_m.get("n") or 1))
    mc = validate_monte_carlo(returns, seed=cfg.seed)
    checks["monte_carlo"] = bool(mc["passed"])
    details["monte_carlo"] = mc

    # Probability calibration still uses labeled outcomes; probabilities from entry frames.
    probs = [float(s["features"].get("pred_probability") or 0.0) for s in (oos or train)]
    outcomes = [int(s["labels"].get("win") or 0) for s in (oos or train)]
    # Apply candidate probability_scale (production weight) before ECE.
    p_scale = float((weights.get("prediction") or {}).get("probability_scale") or 1.0)
    probs = [max(0.0, min(1.0, p * p_scale)) for p in probs]
    cal = validate_probability_calibration(probs, outcomes)
    checks["probability_calibration"] = bool(cal["passed"])
    details["probability_calibration"] = cal

    of = overfit_report(train_m, oos_m if oos_eps else train_m, threshold=cfg.overfit_gap_max)
    checks["no_overfit"] = not bool(of["overfit"])
    details["overfit"] = of

    if shadow_eps:
        shadow_ok = float(shadow_m.get("edge") or 0) >= float(cfg.min_oos_edge) - 0.1
    else:
        shadow_ok = checks["out_of_sample"]
    checks["shadow_paper"] = bool(shadow_ok)
    details["shadow"] = shadow_m

    # Training-only proxy diagnostics (never used for promotion).
    details["training_proxy_diagnostics"] = artifact.get("metrics") or {}

    gate = evaluate_promotion({"checks": checks})
    return {
        "checks": checks,
        "details": details,
        "passed": gate["passed"],
        "reject_reasons": gate["reject_reasons"],
        "metrics": {"train": train_m, "oos": oos_m, "shadow": shadow_m},
    }


def train_and_validate(
    episodes: list[dict[str, Any]],
    models_root: Path,
    *,
    cfg: TrainingConfig | None = None,
) -> dict[str, Any]:
    """Train candidate then validate via production replay (does not auto-promote)."""
    cfg = cfg or TrainingConfig()
    result = train_candidate(episodes, models_root=models_root, cfg=cfg)
    artifact = json.loads(Path(result.artifact_path).read_text(encoding="utf-8"))
    report = validate_candidate_artifact(artifact, episodes, cfg=cfg)
    artifact["validation"] = {
        "passed": report["passed"],
        "checks": report["checks"],
        "reject_reasons": report["reject_reasons"],
        "production_replay": (report.get("details") or {}).get("production_replay"),
    }
    Path(result.artifact_path).write_text(
        json.dumps(artifact, indent=2, sort_keys=True), encoding="utf-8"
    )
    return {
        "training": {
            "model_id": result.model_id,
            "version": result.version,
            "artifact_path": result.artifact_path,
            "config_hash": result.config_hash,
            "dataset_hash": result.dataset_hash,
            "weights_hash": result.weights_hash,
        },
        "validation": report,
    }
