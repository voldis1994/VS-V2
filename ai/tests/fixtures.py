"""Synthetic Stage-7 episode fixtures for Stage-8 learning tests."""
from __future__ import annotations

import copy
from typing import Any


def make_episode(
    *,
    episode_id: str,
    entry_ts: int,
    exit_ts: int | None = None,
    direction: int = 1,
    realized_pnl: float = 1.0,
    mfe: float = 2.0,
    mae: float = 0.5,
    pred_probability: float = 0.6,
    pred_continuation: float = 0.7,
    pred_reversal: float = 0.3,
    pred_ev: float = 0.8,
    peak_retention: float = 0.7,
    post_exit_favorable: float = 0.1,
    post_exit_adverse: float = 0.05,
    exit_action: int = 3,
    n_protect: int = 1,
    n_reduce: int = 0,
    sealed: bool = True,
) -> dict[str, Any]:
    if exit_ts is None:
        exit_ts = entry_ts + 60_000
    actions = []
    for _ in range(n_protect):
        actions.append(
            {
                "action": 1,
                "reason": 1,
                "ev_exit": 0.1,
                "ev_hold": 0.2,
                "continuation_strength": pred_continuation,
                "degradation": pred_reversal,
                "reduce_fraction": 0.0,
                "suggested_stop": 0.0,
                "suggested_target": 0.0,
                "reason_codes": ["protect"],
            }
        )
    for _ in range(n_reduce):
        actions.append(
            {
                "action": 2,
                "reason": 2,
                "ev_exit": 0.2,
                "ev_hold": 0.1,
                "continuation_strength": pred_continuation * 0.8,
                "degradation": pred_reversal * 1.1,
                "reduce_fraction": 0.5,
                "suggested_stop": 0.0,
                "suggested_target": 0.0,
                "reason_codes": ["reduce"],
            }
        )
    actions.append(
        {
            "action": exit_action,
            "reason": 3,
            "ev_exit": 0.3,
            "ev_hold": 0.05,
            "continuation_strength": 0.2,
            "degradation": 0.8,
            "reduce_fraction": 1.0,
            "suggested_stop": 0.0,
            "suggested_target": 0.0,
            "reason_codes": ["exit"],
        }
    )

    side = {
        "direction": direction,
        "continuation": pred_continuation,
        "reversal_failure": pred_reversal,
        "expected_move": abs(mfe),
        "adverse_move": abs(mae),
        "probability": pred_probability,
        "confidence": pred_probability * 0.9,
        "expected_value": pred_ev,
        "invalidation": 0.2,
        "thesis_quality": 0.6,
        "uncertainty": 0.2,
    }
    other = copy.deepcopy(side)
    other["direction"] = 2 if direction == 1 else 1
    other["probability"] = max(0.0, 1.0 - pred_probability)

    frame = {
        "ts": entry_ts,
        "trigger": 2,
        "instrument": 1,
        "prediction": {
            "long_side": side if direction != 2 else other,
            "short_side": side if direction == 2 else other,
            "has_structure_authority": True,
            "has_micro_authority": True,
            "evidence_sufficient": True,
            "structure_volatility": 0.01,
            "structure_invalidation": 0.1,
        },
        "decision_action": 1 if direction == 1 else 2,
        "decision": {
            "instrument": 1,
            "direction": direction,
            "probability": pred_probability,
            "expected_value": pred_ev,
            "spread_cost": 0.01,
            "action": 1 if direction == 1 else 2,
            "stop_distance_frac": 0.005,
            "target_distance_frac": 0.01,
        },
        "risk": {"approved": True, "approved_quantity": 1.0, "confidence": 0.8, "reason_codes": []},
        "execution": {
            "status": 2,
            "requested_quantity": 1.0,
            "filled_quantity": 1.0,
            "fill_price": 100.0,
            "deal_id": f"d-{episode_id}",
            "explanation": "paper",
        },
        "position": {
            "id": 1,
            "intent_id": 1,
            "instrument": 1,
            "direction": direction,
            "entry_price": 100.0,
            "quantity": 1.0,
            "current_price": 100.0 + realized_pnl,
            "opened_at": entry_ts,
            "mfe": mfe,
            "mae": mae,
            "peak_favorable_price": 100.0 + mfe,
            "peak_retention": peak_retention,
            "current_pnl": realized_pnl,
            "stop_loss": 99.0,
            "take_profit": 102.0,
            "deal_id": f"d-{episode_id}",
            "entry_thesis_quality": 0.6,
            "entry_continuation": pred_continuation,
            "entry_invalidation": 0.2,
        },
        "position_decision": actions[-1],
        "has_structure_authority": True,
        "has_micro_authority": True,
        "has_prediction": True,
        "has_decision": True,
        "has_risk": True,
        "has_execution": True,
        "has_position": True,
    }

    return {
        "episode_id": episode_id,
        "instrument": 1,
        "sealed": sealed,
        "provenance": {
            "episode_schema_version": "vs-v2-episode-1",
            "brain_version": "vs-v2-1.0.0",
            "model_id": "default",
            "config_hash": "cfg-test",
            "prediction_weights_hash": "p0",
            "decision_weights_hash": "d0",
            "risk_weights_hash": "r0",
            "execution_weights_hash": "e0",
            "position_weights_hash": "pos0",
        },
        "market": [],
        "frames": [frame],
        "position_actions": actions,
        "outcome": {
            "sealed": sealed,
            "direction": direction,
            "entry_price": 100.0,
            "exit_price": 100.0 + realized_pnl,
            "quantity": 1.0,
            "realized_pnl": realized_pnl,
            "mfe": mfe,
            "mae": mae,
            "peak_retention": peak_retention,
            "exit_action": exit_action,
            "exit_reason": 3,
            "entry_ts": entry_ts,
            "exit_ts": exit_ts,
            "post_exit_favorable": post_exit_favorable,
            "post_exit_adverse": post_exit_adverse,
            "post_exit_end_ts": exit_ts + 30_000,
            "post_exit_samples": 3,
        },
    }


def make_episode_corpus(n: int = 24, *, seed: int = 0) -> list[dict[str, Any]]:
    """Build a time-ordered sealed episode corpus with mixed outcomes."""
    episodes: list[dict[str, Any]] = []
    for i in range(n):
        # Simple deterministic mix driven by seed + index (no RNG for determinism).
        tone = (i * 17 + seed * 13) % 10
        win = tone >= 4
        pnl = (0.5 + (tone % 5) * 0.2) if win else -(0.3 + (tone % 4) * 0.15)
        entry_ts = 1_000_000 + i * 120_000
        exit_ts = entry_ts + 50_000 + (tone % 5) * 1_000
        episodes.append(
            make_episode(
                episode_id=f"ep-{seed:02d}-{i:03d}",
                entry_ts=entry_ts,
                exit_ts=exit_ts,
                direction=1 if (i + seed) % 3 else 2,
                realized_pnl=pnl,
                mfe=abs(pnl) + 0.5 + (tone % 3) * 0.1,
                mae=0.2 + (tone % 4) * 0.05,
                pred_probability=0.45 + (tone % 5) * 0.08,
                pred_continuation=0.4 + (tone % 6) * 0.08,
                pred_reversal=0.6 - (tone % 6) * 0.05,
                pred_ev=pnl * 0.8,
                peak_retention=0.4 + (tone % 5) * 0.1,
                post_exit_favorable=0.05 * ((i + seed) % 4),
                post_exit_adverse=0.04 * ((i + seed * 2) % 3),
                n_protect=1 if tone % 2 == 0 else 0,
                n_reduce=1 if tone % 3 == 0 else 0,
            )
        )
    return episodes


def write_episode_store(root, episodes: list[dict[str, Any]]) -> None:
    import json
    from pathlib import Path

    root = Path(root)
    root.mkdir(parents=True, exist_ok=True)
    for ep in episodes:
        path = root / f"{ep['episode_id']}.episode.json"
        path.write_text(json.dumps(ep, indent=2, sort_keys=True), encoding="utf-8")
