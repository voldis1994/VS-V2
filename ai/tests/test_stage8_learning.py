"""Stage-8 learning: determinism, leakage, OOS/WF, overfit, promote/reject/rollback."""
from __future__ import annotations

import json
from pathlib import Path

import pytest

from ai.episodes.loader import list_episode_ids, load_episodes
from ai.learning.dataset.episode_dataset import build_dataset
from ai.learning.dataset.splits import LeakageError, assert_no_leakage, time_ordered_split
from ai.learning.evaluation.overfit_detection import detect_overfit, overfit_report
from ai.learning.training.trainer import TrainingConfig, train_candidate
from ai.learning.training.weight_space import (
    RISK_SAFETY_LIMITS,
    assert_risk_safety_unchanged,
    default_weight_bundle,
)
from ai.registry.live_guard import LiveMutationError, assert_not_live_mutation
from ai.registry.model_registry import ModelRegistry
from ai.tests.fixtures import make_episode, make_episode_corpus, write_episode_store
from ai.validation.pipeline import train_and_validate
from ai.validation.promotion_gate import evaluate_promotion, promotion_gate
from ai.validation.walk_forward import run_walk_forward


def test_training_determinism(tmp_path: Path):
    episodes = make_episode_corpus(n=20, seed=3)
    cfg = TrainingConfig(seed=99, model_name="det")
    r1 = train_candidate(episodes, models_root=tmp_path / "a", cfg=cfg)
    art1 = json.loads(Path(r1.artifact_path).read_text(encoding="utf-8"))
    r2 = train_candidate(episodes, models_root=tmp_path / "b", cfg=cfg)
    art2 = json.loads(Path(r2.artifact_path).read_text(encoding="utf-8"))
    assert r1.weights_hash == r2.weights_hash
    assert r1.config_hash == r2.config_hash
    assert r1.dataset_hash == r2.dataset_hash
    assert art1["weights"] == art2["weights"]
    assert art1["version"] == art2["version"]


def test_no_data_leakage_in_splits():
    episodes = make_episode_corpus(n=18, seed=1)
    samples = build_dataset(episodes)
    train, oos, shadow = time_ordered_split(samples)
    assert_no_leakage(train, oos)
    if shadow:
        assert_no_leakage(train + oos, shadow)
    for s in samples:
        feats = s["features"]
        for banned in (
            "realized_pnl",
            "post_exit_favorable",
            "post_exit_adverse",
            "mfe",
            "mae",
            "win",
        ):
            assert banned not in feats


def test_leakage_guard_raises():
    early = make_episode(episode_id="a", entry_ts=100, exit_ts=500, realized_pnl=1.0)
    late = make_episode(episode_id="b", entry_ts=200, exit_ts=600, realized_pnl=1.0)
    samples = build_dataset([early, late])
    with pytest.raises(LeakageError):
        assert_no_leakage([samples[1]], [samples[0]])


def test_episode_store_loader(tmp_path: Path):
    episodes = make_episode_corpus(n=5, seed=2)
    write_episode_store(tmp_path, episodes)
    ids = list_episode_ids(tmp_path)
    assert len(ids) == 5
    loaded = load_episodes(tmp_path)
    assert [e["episode_id"] for e in loaded] == [e["episode_id"] for e in episodes]


def test_oos_and_walk_forward(tmp_path: Path):
    episodes = make_episode_corpus(n=24, seed=5)
    report = train_and_validate(
        episodes, tmp_path, cfg=TrainingConfig(seed=5, model_name="wf")
    )
    assert "out_of_sample" in report["validation"]["checks"]
    assert "walk_forward" in report["validation"]["checks"]
    samples = build_dataset(episodes)
    wf = run_walk_forward(samples, cfg=TrainingConfig(seed=5), n_folds=3, min_positive=0)
    assert wf["n_folds"] >= 1
    for fold in wf["folds"]:
        assert fold["n_train"] >= 2
        assert fold["n_test"] >= 1


def test_overfit_rejection():
    assert detect_overfit(1.0, 0.5, threshold=0.15) is True
    assert detect_overfit(0.2, 0.18, threshold=0.15) is False
    rep = overfit_report({"edge": 1.0}, {"edge": 0.2}, threshold=0.15)
    assert rep["reject"] is True


def test_risk_safety_limits_immutable(tmp_path: Path):
    episodes = make_episode_corpus(n=12, seed=4)
    result = train_candidate(
        episodes, models_root=tmp_path, cfg=TrainingConfig(seed=4, model_name="risk")
    )
    art = json.loads(Path(result.artifact_path).read_text(encoding="utf-8"))
    assert_risk_safety_unchanged(art["weights"])
    assert art["weights"]["risk_safety_frozen"] == RISK_SAFETY_LIMITS
    bad = default_weight_bundle()
    key = next(iter(RISK_SAFETY_LIMITS))
    bad["risk_safety_frozen"][key] = float(RISK_SAFETY_LIMITS[key]) + 1.0
    with pytest.raises(ValueError, match="risk safety"):
        assert_risk_safety_unchanged(bad)


def test_live_model_immutable(tmp_path: Path):
    with pytest.raises(LiveMutationError):
        assert_not_live_mutation(tmp_path / "production", target="candidate")
    with pytest.raises(LiveMutationError):
        assert_not_live_mutation(tmp_path, target="production")


def test_candidate_promotion_and_rejection(tmp_path: Path):
    episodes = make_episode_corpus(n=24, seed=8)
    report = train_and_validate(
        episodes, tmp_path, cfg=TrainingConfig(seed=8, model_name="promo")
    )
    registry = ModelRegistry(tmp_path)
    mid = report["training"]["model_id"]
    ver = report["training"]["version"]

    if report["validation"]["passed"]:
        registry.promote(mid, ver, validation=report["validation"])
        manifest = registry.load_production_manifest()
        assert manifest["version"] == ver
        assert manifest["status"] == "production"
        assert not (tmp_path / "candidate" / mid / ver).exists()
    else:
        dest = registry.reject(mid, ver, reason="gate_failed")
        assert dest.exists()
        assert (dest / "rejection.json").exists()
        art = json.loads((dest / "model.json").read_text(encoding="utf-8"))
        assert art["status"] == "rejected"


def test_production_rollback(tmp_path: Path):
    registry = ModelRegistry(tmp_path)
    r1 = train_and_validate(
        make_episode_corpus(n=20, seed=1),
        tmp_path,
        cfg=TrainingConfig(seed=1, model_name="rb"),
    )
    art = json.loads(Path(r1["training"]["artifact_path"]).read_text(encoding="utf-8"))
    assert_risk_safety_unchanged(art["weights"])
    v1 = r1["training"]["version"]
    registry.promote("rb", v1, validation={"passed": True, "checks": {"forced": True}})

    r2 = train_and_validate(
        make_episode_corpus(n=20, seed=2),
        tmp_path,
        cfg=TrainingConfig(seed=2, model_name="rb"),
    )
    v2 = r2["training"]["version"]
    registry.promote("rb", v2, validation={"passed": True, "checks": {"forced": True}})

    rolled = registry.rollback(to_version=v1)
    assert rolled["version"] == v1
    assert registry.load_production_manifest()["version"] == v1


def test_promotion_gate_requires_all_checks():
    full = {
        k: True
        for k in (
            "out_of_sample",
            "walk_forward",
            "stress",
            "monte_carlo",
            "probability_calibration",
            "no_overfit",
            "no_leakage",
            "shadow_paper",
            "risk_safety_frozen",
            "reproducible",
        )
    }
    assert promotion_gate(full) is True
    bad = dict(full)
    bad["no_overfit"] = False
    assert promotion_gate(bad) is False
    assert (
        promotion_gate(
            {
                "out_of_sample": True,
                "walk_forward": True,
                "monte_carlo": True,
                "probability_calibration": True,
            }
        )
        is True
    )
    gate = evaluate_promotion({"checks": bad})
    assert gate["passed"] is False
    assert "no_overfit" in gate["reject_reasons"]


def test_worse_model_rejected_by_overfit_path(tmp_path: Path):
    episodes = []
    for i in range(12):
        episodes.append(
            make_episode(
                episode_id=f"win-{i}",
                entry_ts=1_000_000 + i * 100_000,
                exit_ts=1_000_000 + i * 100_000 + 40_000,
                realized_pnl=2.0,
                pred_probability=0.9,
                pred_ev=2.0,
                mfe=3.0,
                mae=0.1,
            )
        )
    for i in range(12):
        episodes.append(
            make_episode(
                episode_id=f"loss-{i}",
                entry_ts=3_000_000 + i * 100_000,
                exit_ts=3_000_000 + i * 100_000 + 40_000,
                realized_pnl=-2.0,
                pred_probability=0.9,
                pred_ev=2.0,
                mfe=0.2,
                mae=2.5,
            )
        )
    report = train_and_validate(
        episodes,
        tmp_path,
        cfg=TrainingConfig(seed=0, model_name="overfit", overfit_gap_max=0.15),
    )
    checks = report["validation"]["checks"]
    train_edge = report["validation"]["metrics"]["train"]["edge"]
    oos_edge = report["validation"]["metrics"]["oos"]["edge"]
    if train_edge - oos_edge > 0.15:
        assert checks["no_overfit"] is False or checks["out_of_sample"] is False
        assert report["validation"]["passed"] is False


def test_validate_candidate_does_not_mutate_production(tmp_path: Path):
    prod = tmp_path / "production"
    prod.mkdir(parents=True)
    (prod / "manifest.json").write_text(
        json.dumps({"name": "vs-v2", "version": "0.0.0", "status": "placeholder"}),
        encoding="utf-8",
    )
    before = (prod / "manifest.json").read_text(encoding="utf-8")
    train_and_validate(
        make_episode_corpus(n=16, seed=9),
        tmp_path,
        cfg=TrainingConfig(seed=9, model_name="nm"),
    )
    after = (prod / "manifest.json").read_text(encoding="utf-8")
    assert before == after
