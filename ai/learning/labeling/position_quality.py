"""PositionBrain exit / protect / reduce quality labels."""
from __future__ import annotations

from typing import Any

from ai.learning.labeling.exit_quality import exit_quality


# PositionAction: Hold=0, Protect=1, Reduce=2, Exit=3 (C++ enum)
def position_action_quality(ep: dict[str, Any]) -> dict[str, float]:
    outcome = ep.get("outcome") or {}
    mfe = float(outcome.get("mfe") or 0.0)
    pnl = float(outcome.get("realized_pnl") or 0.0)
    peak_ret = float(outcome.get("peak_retention") or 0.0)
    post_fav = float(outcome.get("post_exit_favorable") or 0.0)
    post_adv = float(outcome.get("post_exit_adverse") or 0.0)
    exit_q = exit_quality(mfe, pnl)

    actions = ep.get("position_actions") or []
    n_protect = sum(1 for a in actions if int(a.get("action") or 0) == 1)
    n_reduce = sum(1 for a in actions if int(a.get("action") or 0) == 2)
    n_exit = sum(1 for a in actions if int(a.get("action") or 0) == 3)

    # Good protect/reduce: high peak retention with controlled giveback.
    protect_quality = peak_ret if n_protect else 0.0
    reduce_quality = min(1.0, max(0.0, peak_ret * 0.5 + exit_q * 0.5)) if n_reduce else 0.0
    # Exit too early if large favorable move after exit; too late if large adverse after.
    exit_timing = 0.5
    if post_fav + post_adv > 1e-12:
        # Higher is better (less money left on table, less pain after).
        exit_timing = max(0.0, min(1.0, 1.0 - (post_fav / (post_fav + post_adv + 1e-12))))

    return {
        "exit_quality": exit_q,
        "protect_quality": float(protect_quality),
        "reduce_quality": float(reduce_quality),
        "exit_timing_quality": float(exit_timing),
        "n_protect": float(n_protect),
        "n_reduce": float(n_reduce),
        "n_exit": float(n_exit),
    }
