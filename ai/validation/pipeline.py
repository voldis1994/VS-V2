"""Full candidate validation pipeline: replay → OOS → WF → stress → shadow → gate."""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from ai.learning.dataset.episode_dataset import build_dataset
from ai.learning.dataset.splits import LeakageError, assert_no_leakage, time_ordered_split
from ai.learning.evaluation.overfit_detection import overfit_report
from ai.learning.training.trainer import TrainingConfig, _apply_score, train_candidate
from ai.learning.training.weight_space import assert_risk_safety_unchanged, stable_hash
from ai.validation.monte_carlo import validate_monte_carlo
from ai.validation.out_of_sample import out_of_sample_report
from ai.validation.probability_calibration import validate_probability_calibration
from ai.validation.promotion_gate import evaluate_promotion
from ai.validation.stress import run_stress_suite
from ai.validation.walk_forward import run_walk_forward


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

    # No leakage
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

    # Reproducibility: re-hash weights
    checks["reproducible"] = bool(artifact.get("weights_hash")) and artifact[
        "weights_hash"
    ] == stable_hash(weights)

    train_scored = _apply_score(train, weights)
    oos_scored = _apply_score(oos, weights) if oos else []
    shadow_scored = _apply_score(shadow, weights) if shadow else []

    train_m = (artifact.get("metrics") or {}).get("train") or {}
    oos_m = (artifact.get("metrics") or {}).get("oos") or {}
    # Prefer live recompute for gate honesty
    from ai.learning.evaluation.metrics import score_samples

    train_m = score_samples(train_scored)
    oos_m = score_samples(oos_scored) if oos_scored else {"edge": 0.0, "n": 0}
    shadow_m = score_samples(shadow_scored) if shadow_scored else {"edge": 0.0, "n": 0}

    oos_rep = out_of_sample_report(train_m, oos_m, min_edge=cfg.min_oos_edge)
    checks["out_of_sample"] = bool(oos_rep["passed"])
    details["out_of_sample"] = oos_rep

    wf = run_walk_forward(samples, cfg=cfg, n_folds=3, min_positive=1)
    checks["walk_forward"] = bool(wf["passed"])
    details["walk_forward"] = wf

    stress = run_stress_suite(oos_m, oos_scored)
    checks["stress"] = bool(stress["passed"])
    details["stress"] = stress

    returns = [float(s["score"]["proxy_pnl"]) for s in (oos_scored or train_scored)]
    mc = validate_monte_carlo(returns, seed=cfg.seed)
    checks["monte_carlo"] = bool(mc["passed"])
    details["monte_carlo"] = mc

    probs = [float(s["score"]["calibrated_probability"]) for s in (oos_scored or train_scored)]
    outcomes = [int(s["labels"]["win"]) for s in (oos_scored or train_scored)]
    cal = validate_probability_calibration(probs, outcomes)
    checks["probability_calibration"] = bool(cal["passed"])
    details["probability_calibration"] = cal

    of = overfit_report(train_m, oos_m if oos_scored else train_m, threshold=cfg.overfit_gap_max)
    checks["no_overfit"] = not bool(of["overfit"])
    details["overfit"] = of

    # Shadow / paper gate: shadow edge not catastrophically worse than OOS.
    if shadow_scored:
        shadow_ok = float(shadow_m.get("edge") or 0) >= float(cfg.min_oos_edge) - 0.1
    else:
        # No shadow split available — require OOS pass as paper surrogate.
        shadow_ok = checks["out_of_sample"]
    checks["shadow_paper"] = bool(shadow_ok)
    details["shadow"] = shadow_m

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
    """Train candidate then run full validation (does not auto-promote)."""
    cfg = cfg or TrainingConfig()
    result = train_candidate(episodes, models_root=models_root, cfg=cfg)
    artifact = json.loads(Path(result.artifact_path).read_text(encoding="utf-8"))
    report = validate_candidate_artifact(artifact, episodes, cfg=cfg)
    artifact["validation"] = {
        "passed": report["passed"],
        "checks": report["checks"],
        "reject_reasons": report["reject_reasons"],
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
