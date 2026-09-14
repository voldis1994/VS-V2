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
double neg(double v) { return std::max(0.0, -v); }

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

    const double cont_struct =
        cfg_.w_trend * trend_align
        + cfg_.w_continuation * clamp01(st.continuation_pressure) * (0.5 + 0.5 * swing_align)
        + cfg_.w_pullback * clamp01(st.pullback_depth) * trend_align
        + cfg_.w_breakout * clamp01(st.breakout_strength)
              * ((is_long && st.breakout_up) || (!is_long && st.breakout_down) ? 1.0 : 0.0)
        + cfg_.w_expansion * clamp01(st.expansion) * swing_align;

    const double rev_struct =
        cfg_.w_reversal * clamp01(st.reversal_candidate)
        + cfg_.w_failed_breakout * clamp01(st.failed_breakout)
              * ((is_long && st.failed_breakout_up) || (!is_long && st.failed_breakout_down) ? 1.0
                                                                                           : 0.5)
        + cfg_.w_invalidation * clamp01(st.structural_invalidation)
        + cfg_.w_compression * clamp01(st.compression) * swing_oppose
        + cfg_.w_continuation * swing_oppose * clamp01(st.trend_strength);

    // --- micro evidence (Stage 4 CLOSED 10s) ---
    const double mom = micro.momentum;
    const double mom_align = clamp01(pos(side * mom) * 50.0);  // scale tiny mid-returns softly later
    const double mom_oppose = clamp01(pos(-side * mom) * 50.0);
    const double pressure_align = is_long ? clamp01(micro.buyer_pressure)
                                          : clamp01(micro.seller_pressure);
    const double pressure_oppose = is_long ? clamp01(micro.seller_pressure)
                                           : clamp01(micro.buyer_pressure);

    const double cont_micro =
        cfg_.w_micro_momentum * soft01(pos(side * mom), cfg_.continuation_scale * 0.002)
        + cfg_.w_micro_continuation * clamp01(micro.continuation)
              * soft01(pos(side * mom) + 1e-9, 0.002)
        + cfg_.w_micro_pressure * pressure_align
        + cfg_.w_micro_acceptance * clamp01(micro.acceptance) * pressure_align
        + cfg_.w_micro_reclaim * clamp01(micro.reclaim) * pressure_align
        + cfg_.w_micro_timing * clamp01(micro.entry_timing_quality) * pressure_align
        + cfg_.w_breakout * clamp01(micro.breakout_strength)
              * ((is_long && micro.breakout_up) || (!is_long && micro.breakout_down) ? 1.0 : 0.0);

    // Strict directional alignment — no large floor that invents opposite-side continuation.
    const double align = soft01(pos(side * mom) + pressure_align * 0.01, 0.002);
    const double cont_micro_aligned = cont_micro * align;

    const double rev_micro =
        cfg_.w_micro_exhaustion * clamp01(micro.exhaustion)
        + cfg_.w_micro_rejection * clamp01(micro.rejection)
        + cfg_.w_micro_failed_breakout * clamp01(micro.failed_breakout)
              * ((is_long && micro.failed_breakout_up) || (!is_long && micro.failed_breakout_down)
                     ? 1.0
                     : 0.5)
        + cfg_.w_micro_pressure * pressure_oppose
        + cfg_.w_micro_momentum * soft01(pos(-side * mom), cfg_.reversal_scale * 0.002);

    // Optional soft dynamics context (not RAW authority)
    const double dyn_align =
        soft01(pos(side * pd.directional_persistence), cfg_.continuation_scale);
    const double dyn_oppose =
        soft01(pos(-side * pd.directional_persistence), cfg_.reversal_scale);

    const double cont_raw = cont_struct + cont_micro_aligned + 0.25 * dyn_align;
    const double rev_raw = rev_struct + rev_micro + 0.25 * dyn_oppose;

    out.continuation = soft01(cont_raw, cfg_.continuation_scale);
    out.reversal_failure = soft01(rev_raw, cfg_.reversal_scale);

    // Expected / adverse move magnitudes (normalized continuous)
    const double move_raw =
        out.continuation * (0.4 + 0.6 * clamp01(st.expansion + micro.expansion + st.breakout_strength))
        + 0.2 * clamp01(st.volatility + micro.volatility);
    const double adverse_raw =
        out.reversal_failure
            * (0.4 + 0.6 * clamp01(st.volatility + micro.volatility + st.structural_invalidation))
        + 0.2 * clamp01(micro.exhaustion + micro.rejection);
    out.expected_move = soft01(move_raw, cfg_.move_scale);
    out.adverse_move = soft01(adverse_raw, cfg_.adverse_scale);

    // Soft probability from relative continuation vs reversal (no absolute gate)
    const double cont_mass = soft01(out.continuation, cfg_.probability_scale);
    const double rev_mass = soft01(out.reversal_failure, cfg_.probability_scale);
    const double denom = cont_mass + rev_mass + 1e-9;
    out.probability = cont_mass / denom;

    out.invalidation = clamp01(0.5 * out.reversal_failure
                               + 0.3 * clamp01(st.structural_invalidation)
                               + 0.2 * clamp01(micro.exhaustion));
    out.uncertainty = clamp01(1.0 - std::abs(cont_mass - rev_mass) / denom);

    const double quality_raw =
        cfg_.w_structure_quality * clamp01(st.structure_quality)
        + clamp01(micro.entry_timing_quality) * cfg_.w_micro_timing
        + out.continuation * (1.0 - out.uncertainty);
    out.confidence = soft01(quality_raw * (1.0 - 0.5 * out.uncertainty), cfg_.confidence_scale);
    out.thesis_quality = clamp01(out.confidence * (1.0 - out.invalidation) * out.continuation);

    out.expected_value =
        out.probability * out.expected_move - (1.0 - out.probability) * out.adverse_move;

    (void)mom_align;
    (void)mom_oppose;
    return out;
}

DualPrediction PredictionEngine::evaluate(const StructureFeatures& structure,
                                          bool has_structure_authority,
                                          const MicrostructureFeatures& micro,
                                          const PriceDynamics& pd) const {
    DualPrediction dual;
    dual.has_structure_authority = has_structure_authority;
    dual.has_micro_authority = micro.has_authority && micro.setup_confirmed;

    dual.long_side = evaluate_side(Direction::Long, structure, micro, pd);
    dual.short_side = evaluate_side(Direction::Short, structure, micro, pd);

    // Sufficient evidence requires both authorities (structure context + 10s timing).
    dual.evidence_sufficient = dual.has_structure_authority && dual.has_micro_authority;
    if (!dual.evidence_sufficient) {
        dual.long_side.thesis_quality = 0;
        dual.short_side.thesis_quality = 0;
        dual.long_side.confidence *= 0.25;
        dual.short_side.confidence *= 0.25;
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
    // Map scenario soft prior into a minimal structure stub (continuous, not a trigger).
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
    micro.momentum = (dir == Direction::Long ? 1.0 : -1.0) * 0.001 * scenario.confidence;
    micro.buyer_pressure = dir == Direction::Long ? 0.6 : 0.4;
    micro.seller_pressure = 1.0 - micro.buyer_pressure;

    const auto side = evaluate_side(dir, st, micro, pd);
    p.probability = side.probability;
    p.uncertainty = side.uncertainty;
    p.expected_mfe = side.expected_move;
    p.expected_mae = side.adverse_move;
    p.expected_duration_s = 30.0 + side.confidence * 60.0;
    return p;
}

}  // namespace mr
