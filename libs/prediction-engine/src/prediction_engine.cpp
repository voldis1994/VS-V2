#include "mr/prediction_engine/prediction_engine.hpp"
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

double pos(double v) { return std::max(0.0, v); }

double wsum2(double a, double b, double wa, double wb) {
    return wa * a + wb * b;
}

double wavg2(double a, double b, double wa, double wb) {
    const double den = std::max(1e-15, wa + wb);
    return (wa * a + wb * b) / den;
}

double wavg3(double a, double b, double c, double wa, double wb, double wc) {
    const double den = std::max(1e-15, wa + wb + wc);
    return (wa * a + wb * b + wc * c) / den;
}

}  // namespace

PredictionEngine::PredictionEngine(PredictionWeightConfig cfg) : cfg_(std::move(cfg)) {}

void PredictionEngine::set_weight_config(PredictionWeightConfig cfg) { cfg_ = std::move(cfg); }

SidePrediction PredictionEngine::evaluate_side(Direction dir,
                                               const StructureFeatures& st,
                                               const MicrostructureFeatures& micro,
                                               const PriceDynamics& pd) const {
    SidePrediction out;
    out.direction = dir;

    const bool is_long = dir == Direction::Long;
    const double side = is_long ? 1.0 : -1.0;

    // --- structure evidence (Stage 3 authority) ---
    const double trend_align =
        (st.trend_direction == TrendBias::Up)   ? (is_long ? st.trend_strength : 0.0)
      : (st.trend_direction == TrendBias::Down) ? (is_long ? 0.0 : st.trend_strength)
      : 0.0;
    const double swing_align = clamp01(pos(side * st.swing_state));
    const double swing_oppose = clamp01(pos(-side * st.swing_state));
    const double swing_couple =
        wavg2(1.0, swing_align, cfg_.swing_couple_base, cfg_.swing_couple_align);

    const double breakout_align =
        ((is_long && st.breakout_up) || (!is_long && st.breakout_down)) ? 1.0 : 0.0;
    const double failed_align =
        ((is_long && st.failed_breakout_up) || (!is_long && st.failed_breakout_down))
            ? 1.0
            : cfg_.failed_breakout_mismatch;

    const double cont_struct =
        cfg_.w_trend * trend_align
        + cfg_.w_continuation * clamp01(st.continuation_pressure) * swing_couple
        + cfg_.w_pullback * clamp01(st.pullback_depth) * trend_align
        + cfg_.w_breakout * clamp01(st.breakout_strength) * breakout_align
        + cfg_.w_expansion * clamp01(st.expansion) * swing_align;

    const double rev_struct =
        cfg_.w_reversal * clamp01(st.reversal_candidate)
        + cfg_.w_failed_breakout * clamp01(st.failed_breakout) * failed_align
        + cfg_.w_invalidation * clamp01(st.structural_invalidation)
        + cfg_.w_compression * clamp01(st.compression) * swing_oppose
        + cfg_.w_continuation * swing_oppose * clamp01(st.trend_strength);

    // --- micro evidence (Stage 4 CLOSED 10s) ---
    const double mom = micro.momentum;
    const double pressure_align =
        is_long ? clamp01(micro.buyer_pressure) : clamp01(micro.seller_pressure);
    const double pressure_oppose =
        is_long ? clamp01(micro.seller_pressure) : clamp01(micro.buyer_pressure);

    const double mom_soft = soft01(pos(side * mom), cfg_.momentum_soft_scale);
    const double mom_oppose_soft = soft01(pos(-side * mom), cfg_.momentum_soft_scale);

    const double micro_breakout_align =
        ((is_long && micro.breakout_up) || (!is_long && micro.breakout_down)) ? 1.0 : 0.0;
    const double micro_failed_align =
        ((is_long && micro.failed_breakout_up) || (!is_long && micro.failed_breakout_down))
            ? 1.0
            : cfg_.failed_breakout_mismatch;

    const double cont_micro =
        cfg_.w_micro_momentum * mom_soft
        + cfg_.w_micro_continuation * clamp01(micro.continuation) * mom_soft
        + cfg_.w_micro_pressure * pressure_align
        + cfg_.w_micro_acceptance * clamp01(micro.acceptance) * pressure_align
        + cfg_.w_micro_reclaim * clamp01(micro.reclaim) * pressure_align
        + cfg_.w_micro_timing * clamp01(micro.entry_timing_quality) * pressure_align
        + cfg_.w_breakout * clamp01(micro.breakout_strength) * micro_breakout_align;

    // Strict directional alignment from continuous evidence only.
    const double align = soft01(pos(side * mom) + cfg_.align_pressure_weight * pressure_align,
                                cfg_.align_soft_scale);
    const double cont_micro_aligned = cont_micro * align;

    const double rev_micro =
        cfg_.w_micro_exhaustion * clamp01(micro.exhaustion)
        + cfg_.w_micro_rejection * clamp01(micro.rejection)
        + cfg_.w_micro_failed_breakout * clamp01(micro.failed_breakout) * micro_failed_align
        + cfg_.w_micro_pressure * pressure_oppose
        + cfg_.w_micro_momentum * mom_oppose_soft;

    // Optional soft dynamics context (not RAW authority)
    const double dyn_align =
        soft01(pos(side * pd.directional_persistence), cfg_.continuation_scale);
    const double dyn_oppose =
        soft01(pos(-side * pd.directional_persistence), cfg_.reversal_scale);

    const double cont_raw = cont_struct + cont_micro_aligned + cfg_.w_dynamics * dyn_align;
    const double rev_raw = rev_struct + rev_micro + cfg_.w_dynamics * dyn_oppose;

    out.continuation = soft01(cont_raw, cfg_.continuation_scale);
    out.reversal_failure = soft01(rev_raw, cfg_.reversal_scale);

    const double expansion_ev = clamp01(st.expansion + micro.expansion + st.breakout_strength);
    const double vol_ev = clamp01(st.volatility + micro.volatility);
    const double stress_ev =
        clamp01(st.volatility + micro.volatility + st.structural_invalidation);
    const double reject_ev = clamp01(micro.exhaustion + micro.rejection);

    const double move_raw =
        out.continuation * wavg2(1.0, expansion_ev, cfg_.move_base_weight, cfg_.move_expansion_weight)
        + cfg_.move_vol_weight * vol_ev;
    const double adverse_raw =
        out.reversal_failure
            * wavg2(1.0, stress_ev, cfg_.adverse_base_weight, cfg_.adverse_stress_weight)
        + cfg_.adverse_reject_weight * reject_ev;

    out.expected_move = soft01(move_raw, cfg_.move_scale);
    out.adverse_move = soft01(adverse_raw, cfg_.adverse_scale);

    // Soft probability from relative continuation vs reversal (no absolute gate)
    const double cont_mass = soft01(out.continuation, cfg_.probability_scale);
    const double rev_mass = soft01(out.reversal_failure, cfg_.probability_scale);
    const double denom = cont_mass + rev_mass + 1e-9;
    out.probability = cont_mass / denom;

    out.invalidation = clamp01(wavg3(out.reversal_failure,
                                     clamp01(st.structural_invalidation),
                                     clamp01(micro.exhaustion),
                                     cfg_.inv_reversal_weight,
                                     cfg_.inv_structure_weight,
                                     cfg_.inv_exhaustion_weight));
    out.uncertainty = clamp01(1.0 - std::abs(cont_mass - rev_mass) / denom);

    const double quality_raw =
        cfg_.w_structure_quality * clamp01(st.structure_quality)
        + clamp01(micro.entry_timing_quality) * cfg_.w_micro_timing
        + out.continuation * (1.0 - out.uncertainty);
    const double conf_dampen =
        1.0 - clamp01(cfg_.uncertainty_confidence_weight * out.uncertainty);
    out.confidence = soft01(quality_raw * conf_dampen, cfg_.confidence_scale);
    out.thesis_quality = clamp01(out.confidence * (1.0 - out.invalidation) * out.continuation);

    out.expected_value =
        out.probability * out.expected_move - (1.0 - out.probability) * out.adverse_move;

    return out;
}

DualPrediction PredictionEngine::evaluate(const StructureFeatures& structure,
                                          bool has_structure_authority,
                                          const MicrostructureFeatures& micro,
                                          const PriceDynamics& pd) const {
    DualPrediction dual;
    dual.has_structure_authority = has_structure_authority;
    dual.has_micro_authority = micro.has_authority && micro.setup_confirmed;
    dual.structure_volatility = clamp01(structure.volatility);
    dual.structure_invalidation = clamp01(structure.structural_invalidation);

    dual.long_side = evaluate_side(Direction::Long, structure, micro, pd);
    dual.short_side = evaluate_side(Direction::Short, structure, micro, pd);

    dual.evidence_sufficient = dual.has_structure_authority && dual.has_micro_authority;
    if (!dual.evidence_sufficient) {
        dual.long_side.thesis_quality = 0;
        dual.short_side.thesis_quality = 0;
        dual.long_side.confidence *= cfg_.insufficient_confidence_scale;
        dual.short_side.confidence *= cfg_.insufficient_confidence_scale;
    }
    return dual;
}

Prediction PredictionEngine::predict(const Scenario& scenario,
                                     const PriceDynamics& pd,
                                     Direction dir) const {
    // Legacy adapter: scenario confidence as soft prior, dynamics as context.
    // Not the Stage-5 authority path and not an order.
    Prediction p;
    StructureFeatures st;
    MicrostructureFeatures micro;
    st.continuation_pressure = clamp01(scenario.confidence);
    st.structure_quality = clamp01(scenario.confidence);
    st.trend_strength = clamp01(scenario.confidence);
    if (dir == Direction::Long) st.trend_direction = TrendBias::Up;
    else if (dir == Direction::Short) st.trend_direction = TrendBias::Down;
    st.breakout_strength = clamp01(scenario.target_move);
    st.structural_invalidation = clamp01(scenario.invalidation);
    st.bar_count = 1;
    st.swing_count = 1;

    micro.has_authority = true;
    micro.setup_confirmed = true;
    micro.continuation = clamp01(scenario.confidence);
    micro.entry_timing_quality = clamp01(scenario.confidence);
    micro.momentum =
        (dir == Direction::Long ? 1.0 : -1.0) * cfg_.legacy_momentum_scale * scenario.confidence;
    micro.buyer_pressure =
        dir == Direction::Long ? cfg_.legacy_pressure_long : cfg_.legacy_pressure_short;
    micro.seller_pressure = 1.0 - micro.buyer_pressure;

    const auto side = evaluate_side(dir, st, micro, pd);
    p.probability = side.probability;
    p.uncertainty = side.uncertainty;
    p.expected_mfe = side.expected_move;
    p.expected_mae = side.adverse_move;
    p.expected_duration_s =
        cfg_.legacy_duration_base_s + side.confidence * cfg_.legacy_duration_span_s;
    return p;
}

}  // namespace mr
