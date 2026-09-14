#!/usr/bin/env python3
"""CI validation entrypoint — Stage-8 train → validate → gate (promote/reject/rollback)."""
from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai.learning.training.trainer import TrainingConfig
from ai.registry.model_registry import ModelRegistry
from ai.tests.fixtures import make_episode_corpus
from ai.validation.pipeline import train_and_validate
from ai.validation.promotion_gate import promotion_gate


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        models_root = Path(tmp) / "models"
        episodes = make_episode_corpus(n=24, seed=7)
        report = train_and_validate(
            episodes, models_root, cfg=TrainingConfig(seed=7, model_name="vs-v2-ci")
        )
        checks = report["validation"]["checks"]
        gate_ok = bool(report["validation"]["passed"]) and promotion_gate(checks)

        reject_demo = promotion_gate(
            {
                "out_of_sample": False,
                "walk_forward": True,
                "monte_carlo": True,
                "probability_calibration": True,
                "stress": True,
                "no_overfit": False,
                "no_leakage": True,
                "shadow_paper": True,
                "risk_safety_frozen": True,
                "reproducible": True,
                "production_replay": True,
            }
        )
        if reject_demo:
            print("FAIL: overfit/oos-fail candidate incorrectly passed promotion_gate")
            return 1

        # Legacy four-key bypass must no longer promote.
        if promotion_gate(
            {
                "out_of_sample": True,
                "walk_forward": True,
                "monte_carlo": True,
                "probability_calibration": True,
            }
        ):
            print("FAIL: legacy four-key bypass incorrectly passed promotion_gate")
            return 1

        registry = ModelRegistry(models_root)
        training = report["training"]
        if gate_ok:
            registry.promote(
                training["model_id"],
                training["version"],
                validation=report["validation"],
            )
            report2 = train_and_validate(
                make_episode_corpus(n=24, seed=11),
                models_root,
                cfg=TrainingConfig(seed=11, model_name="vs-v2-ci"),
            )
            if report2["validation"]["passed"]:
                v1 = training["version"]
                registry.promote(
                    report2["training"]["model_id"],
                    report2["training"]["version"],
                    validation=report2["validation"],
                )
                rolled = registry.rollback(to_version=v1)
                if rolled["version"] != v1:
                    print("FAIL: rollback did not restore prior production version")
                    return 1
            print(
                json.dumps(
                    {
                        "promotion_gate": True,
                        "validation_passed": True,
                        "rollback_ok": True,
                        "checks": checks,
                    },
                    indent=2,
                    sort_keys=True,
                )
            )
            return 0

        registry.reject(
            training["model_id"],
            training["version"],
            reason=",".join(report["validation"].get("reject_reasons") or ["gate_failed"]),
        )
        print(
            json.dumps(
                {
                    "promotion_gate": False,
                    "validation_passed": False,
                    "rejected": True,
                    "reject_reasons": report["validation"].get("reject_reasons"),
                    "checks": checks,
                },
                indent=2,
                sort_keys=True,
            )
        )
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
