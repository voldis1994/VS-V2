#include "mr/market_core/weight_bundle.hpp"
#include "mr/market_core/pipeline.hpp"

namespace mr {
namespace {

double jd(const nlohmann::json& j, const char* key, double fallback) {
    if (!j.contains(key) || j.at(key).is_null()) return fallback;
    return j.at(key).get<double>();
}

PredictionWeightConfig prediction_from_json(const nlohmann::json& j) {
    auto c = PredictionWeightConfig::defaults();
    if (j.is_null() || !j.is_object()) return c;
    c.adverse_base_weight = jd(j, "adverse_base_weight", c.adverse_base_weight);
    c.adverse_reject_weight = jd(j, "adverse_reject_weight", c.adverse_reject_weight);
    c.adverse_scale = jd(j, "adverse_scale", c.adverse_scale);
    c.adverse_stress_weight = jd(j, "adverse_stress_weight", c.adverse_stress_weight);
    c.align_pressure_weight = jd(j, "align_pressure_weight", c.align_pressure_weight);
    c.align_soft_scale = jd(j, "align_soft_scale", c.align_soft_scale);
    c.confidence_scale = jd(j, "confidence_scale", c.confidence_scale);
    c.continuation_scale = jd(j, "continuation_scale", c.continuation_scale);
    c.failed_breakout_mismatch = jd(j, "failed_breakout_mismatch", c.failed_breakout_mismatch);
    c.insufficient_confidence_scale =
        jd(j, "insufficient_confidence_scale", c.insufficient_confidence_scale);
    c.inv_exhaustion_weight = jd(j, "inv_exhaustion_weight", c.inv_exhaustion_weight);
    c.inv_reversal_weight = jd(j, "inv_reversal_weight", c.inv_reversal_weight);
    c.inv_structure_weight = jd(j, "inv_structure_weight", c.inv_structure_weight);
    c.probability_scale = jd(j, "probability_scale", c.probability_scale);
    c.reversal_scale = jd(j, "reversal_scale", c.reversal_scale);
    c.w_continuation = jd(j, "w_continuation", c.w_continuation);
    c.w_reversal = jd(j, "w_reversal", c.w_reversal);
    c.w_trend = jd(j, "w_trend", c.w_trend);
    return c;
}

DecisionWeightConfig decision_from_json(const nlohmann::json& j) {
    auto c = DecisionWeightConfig::defaults();
    if (j.is_null() || !j.is_object()) return c;
    c.edge_scale = jd(j, "edge_scale", c.edge_scale);
    c.conflict_scale = jd(j, "conflict_scale", c.conflict_scale);
    c.weakness_scale = jd(j, "weakness_scale", c.weakness_scale);
    c.cost_scale = jd(j, "cost_scale", c.cost_scale);
    c.w_quality = jd(j, "w_quality", c.w_quality);
    c.w_continuation = jd(j, "w_continuation", c.w_continuation);
    return c;
}

ExecutionWeightConfig execution_from_json(const nlohmann::json& j) {
    auto c = ExecutionWeightConfig::defaults();
    if (j.is_null() || !j.is_object()) return c;
    c.max_attempts = static_cast<std::uint32_t>(jd(j, "max_attempts", c.max_attempts));
    c.backoff_ms = static_cast<std::uint64_t>(jd(j, "backoff_ms", static_cast<double>(c.backoff_ms)));
    c.dedup_window_ms =
        static_cast<std::uint64_t>(jd(j, "dedup_window_ms", static_cast<double>(c.dedup_window_ms)));
    c.require_positive_quantity = jd(j, "require_positive_quantity", 1.0) >= 0.5;
    return c;
}

PositionWeightConfig position_from_json(const nlohmann::json& j) {
    auto c = PositionWeightConfig::defaults();
    if (j.is_null() || !j.is_object()) return c;
    c.w_continuation = jd(j, "w_continuation", c.w_continuation);
    c.w_invalidation = jd(j, "w_invalidation", c.w_invalidation);
    c.w_reversal = jd(j, "w_reversal", c.w_reversal);
    c.w_thesis_quality = jd(j, "w_thesis_quality", c.w_thesis_quality);
    c.w_mfe = jd(j, "w_mfe", c.w_mfe);
    c.w_mae = jd(j, "w_mae", c.w_mae);
    c.w_peak_retention = jd(j, "w_peak_retention", c.w_peak_retention);
    c.continuation_scale = jd(j, "continuation_scale", c.continuation_scale);
    c.degradation_scale = jd(j, "degradation_scale", c.degradation_scale);
    c.protect_scale = jd(j, "protect_scale", c.protect_scale);
    c.reduce_scale = jd(j, "reduce_scale", c.reduce_scale);
    c.exit_scale = jd(j, "exit_scale", c.exit_scale);
    c.mfe_scale = jd(j, "mfe_scale", c.mfe_scale);
    c.mae_scale = jd(j, "mae_scale", c.mae_scale);
    c.reduce_fraction = jd(j, "reduce_fraction", c.reduce_fraction);
    c.w_dynamics = jd(j, "w_dynamics", c.w_dynamics);
    return c;
}

}  // namespace

WeightBundle WeightBundle::defaults() { return WeightBundle{}; }

WeightBundle WeightBundle::from_json(const nlohmann::json& j) {
    WeightBundle b = defaults();
    if (j.contains("prediction")) b.prediction = prediction_from_json(j.at("prediction"));
    if (j.contains("decision")) b.decision = decision_from_json(j.at("decision"));
    if (j.contains("execution")) b.execution = execution_from_json(j.at("execution"));
    if (j.contains("position")) b.position = position_from_json(j.at("position"));
    if (j.contains("risk_soft")) {
        const auto& soft = j.at("risk_soft");
        b.risk_spread_cost_scale = jd(soft, "spread_cost_scale", b.risk_spread_cost_scale);
        b.risk_min_net_ev_scale = jd(soft, "min_net_ev_scale", b.risk_min_net_ev_scale);
    }
    return b;
}

void apply_weight_bundle(MarketCorePipeline& pipeline, const WeightBundle& bundle) {
    pipeline.prediction_engine().set_weight_config(bundle.prediction);
    pipeline.decision_engine().set_weight_config(bundle.decision);
    pipeline.position_brain().set_weight_config(bundle.position);
    if (pipeline.execution() != nullptr) {
        pipeline.execution()->set_weight_config(bundle.execution);
    }
    // Risk: start from defaults (safety immutable), then apply soft scales only.
    auto risk = RiskWeightConfig::defaults();
    risk.spread_cost_scale = bundle.risk_spread_cost_scale;
    risk.min_net_ev_scale = bundle.risk_min_net_ev_scale;
    pipeline.risk_engine().set_weight_config(risk);
}

}  // namespace mr
