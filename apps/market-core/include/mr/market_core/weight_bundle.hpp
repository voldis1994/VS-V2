#pragma once

#include "mr/decision/decision_weight_config.hpp"
#include "mr/execution_engine/execution_weight_config.hpp"
#include "mr/position_brain/position_weight_config.hpp"
#include "mr/prediction_engine/prediction_weight_config.hpp"
#include "mr/risk/risk_weight_config.hpp"

#include <nlohmann/json.hpp>

namespace mr {

/**
 * Stage-8 candidate weight bundle for production EpisodeReplay evaluation.
 * Risk safety limits always forced to RiskWeightConfig::defaults();
 * only soft risk scales may come from the candidate.
 */
struct WeightBundle {
    PredictionWeightConfig prediction{PredictionWeightConfig::defaults()};
    DecisionWeightConfig decision{DecisionWeightConfig::defaults()};
    ExecutionWeightConfig execution{ExecutionWeightConfig::defaults()};
    PositionWeightConfig position{PositionWeightConfig::defaults()};
    double risk_spread_cost_scale{1.0};
    double risk_min_net_ev_scale{1.0};

    static WeightBundle defaults();
    static WeightBundle from_json(const nlohmann::json& j);
};

class MarketCorePipeline;
void apply_weight_bundle(MarketCorePipeline& pipeline, const WeightBundle& bundle);

}  // namespace mr
