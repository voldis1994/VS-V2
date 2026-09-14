#include "mr/position_brain/position_brain.hpp"
#include <algorithm>
#include <cmath>

namespace mr {
namespace {

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

double soft01(double raw, double scale) {
    const double x = std::max(0.0, raw);
    if (scale <= 1e-15) return clamp01(x);
    return clamp01(1.0 - std::exp(-x / scale));
}

}  // namespace

PositionBrain::PositionBrain(PositionWeightConfig cfg) : cfg_(std::move(cfg)) {}

void PositionBrain::set_weight_config(PositionWeightConfig cfg) { cfg_ = std::move(cfg); }

SidePrediction PositionBrain::side_for(const DualPrediction& dual, Direction direction) {
    return direction == Direction::Short ? dual.short_side : dual.long_side;
}

double PositionBrain::pnl(const PositionState& p, double price) const {
    if (p.direction == Direction::Long) return (price - p.entry_price) * p.quantity;
    if (p.direction == Direction::Short) return (p.entry_price - price) * p.quantity;
    return 0.0;
}

PositionState PositionBrain::open(const TradeIntent& intent,
                                  double fill_price,
                                  double qty,
                                  const SidePrediction& entry_thesis) {
    PositionState p;
    p.id = ids_.generate();
    p.intent_id = intent.id;
    p.instrument = intent.instrument;
    p.direction = intent.direction;
    p.entry_price = fill_price;
    p.quantity = qty;
    p.current_price = fill_price;
    p.opened_at = intent.created_at;
    p.stop_loss = intent.stop_loss;
    p.take_profit = intent.take_profit;
    p.peak_favorable_price = fill_price;
    p.entry_thesis_quality = entry_thesis.thesis_quality;
    p.entry_continuation = entry_thesis.continuation;
    p.entry_invalidation = entry_thesis.invalidation;
    return p;
}

void PositionBrain::update_excursions(PositionState& pos, double price) const {
    pos.current_price = price;
    pos.current_pnl = pnl(pos, price);
    const double fav = pos.direction == Direction::Long ? (price - pos.entry_price)
                                                        : (pos.entry_price - price);
    const double adv = pos.direction == Direction::Long ? (pos.entry_price - price)
                                                        : (price - pos.entry_price);
    if (fav > pos.mfe) {
        pos.mfe = fav;
        pos.peak_favorable_price = price;
    }
    if (adv > pos.mae) pos.mae = adv;
    pos.peak_retention = pos.mfe > 0.0 ? clamp01(fav / pos.mfe) : 0.0;
}

PositionDecision PositionBrain::evaluate(PositionState& pos,
                                         const SidePrediction& side_now,
                                         const PriceDynamics& pd) const {
    update_excursions(pos, pos.current_price);
    PositionDecision d;
    d.suggested_stop = pos.stop_loss;
    d.suggested_target = pos.take_profit;

    const double cont =
        soft01(cfg_.w_continuation * side_now.continuation, cfg_.continuation_scale);
    const double inv = clamp01(cfg_.w_invalidation * side_now.invalidation);
    const double rev = clamp01(cfg_.w_reversal * side_now.reversal_failure);
    const double q_now = clamp01(cfg_.w_thesis_quality * side_now.thesis_quality);
    const double q_entry = clamp01(pos.entry_thesis_quality);
    const double thesis_drop = clamp01(q_entry - q_now);

    const double dyn_against =
        pos.direction == Direction::Long
            ? soft01(std::max(0.0, -pd.velocity), cfg_.dynamics_velocity_scale)
            : pos.direction == Direction::Short
                  ? soft01(std::max(0.0, pd.velocity), cfg_.dynamics_velocity_scale)
                  : 0.0;

    const double degradation = soft01(
        cfg_.w_invalidation * inv
            + cfg_.w_reversal * rev
            + cfg_.w_thesis_quality * thesis_drop
            + cfg_.w_mae * soft01(pos.mae, cfg_.mae_scale)
            + cfg_.w_dynamics * dyn_against,
        cfg_.degradation_scale);

    const double continuation_strength = clamp01(cont * (1.0 - degradation));
    d.continuation_strength = continuation_strength;
    d.degradation = degradation;
    d.ev_hold = continuation_strength * side_now.expected_value
                - degradation * side_now.adverse_move;
    d.ev_exit = degradation * side_now.expected_value
                - continuation_strength * side_now.adverse_move;

    const bool hit_stop =
        (pos.direction == Direction::Long && pos.stop_loss > 0.0
         && pos.current_price <= pos.stop_loss)
        || (pos.direction == Direction::Short && pos.stop_loss > 0.0
            && pos.current_price >= pos.stop_loss);
    const bool hit_target =
        (pos.direction == Direction::Long && pos.take_profit > 0.0
         && pos.current_price >= pos.take_profit)
        || (pos.direction == Direction::Short && pos.take_profit > 0.0
            && pos.current_price <= pos.take_profit);

    // Hard geometry still wins — continuous scores handle soft management.
    if (hit_stop) {
        d.action = PositionAction::Exit;
        d.reason = ExitReason::HardInvalidation;
        d.reason_codes.push_back("STOP");
        return d;
    }
    if (hit_target) {
        d.action = PositionAction::Exit;
        d.reason = ExitReason::Target;
        d.reason_codes.push_back("TARGET");
        return d;
    }

    const double exit_score = soft01(degradation + inv + rev, cfg_.exit_scale);
    const double protect_score = soft01(
        (1.0 - pos.peak_retention) * cfg_.w_peak_retention
            + degradation * cfg_.protect_degradation_mix
            + (1.0 - continuation_strength),
        cfg_.protect_scale);
    const double reduce_score = soft01(
        degradation
                * (cfg_.reduce_degradation_base
                   + cfg_.reduce_mfe_mix * soft01(pos.mfe, cfg_.mfe_scale))
            + thesis_drop,
        cfg_.reduce_scale);
    const double hold_score = clamp01(continuation_strength * (1.0 - exit_score));

    const double best = std::max({hold_score, protect_score, reduce_score, exit_score});
    if (best == exit_score && exit_score >= hold_score) {
        d.action = PositionAction::Exit;
        d.reason = inv >= rev ? ExitReason::HardInvalidation : ExitReason::ThesisFailure;
        d.reason_codes.push_back("THESIS_EXIT");
        return d;
    }
    if (best == reduce_score && reduce_score > hold_score) {
        d.action = PositionAction::Reduce;
        d.reason = ExitReason::ReversalEvidence;
        d.reduce_fraction = clamp01(cfg_.reduce_fraction * reduce_score);
        d.reason_codes.push_back("REDUCE");
        return d;
    }
    if (best == protect_score && protect_score > hold_score) {
        d.action = PositionAction::Protect;
        d.reason = ExitReason::PeakProtection;
        if (pos.mfe > 0.0) {
            const double tighten = pos.mfe * clamp01(cfg_.stop_tighten_frac) * protect_score;
            if (pos.direction == Direction::Long) {
                d.suggested_stop = std::max(pos.stop_loss, pos.entry_price + tighten);
            } else if (pos.direction == Direction::Short) {
                d.suggested_stop = (pos.stop_loss > 0.0)
                    ? std::min(pos.stop_loss, pos.entry_price - tighten)
                    : (pos.entry_price - tighten);
            }
        }
        d.reason_codes.push_back("PROTECT");
        return d;
    }

    d.action = PositionAction::Hold;
    d.reason = ExitReason::None;
    d.reason_codes.push_back("HOLD");
    return d;
}

}  // namespace mr
