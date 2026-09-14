#pragma once
#include "mr/brain_core/market_brain.hpp"
#include "mr/normalization/normalizer.hpp"
#include "mr/data_quality/quality_engine.hpp"
#include "mr/feed_fusion/feed_fusion_engine.hpp"
#include "mr/perception_engine/perception_engine.hpp"
#include "mr/structure_engine/structure_engine.hpp"
#include "mr/market_concepts/market_concepts_engine.hpp"
#include "mr/microstructure_engine/microstructure_engine.hpp"
#include "mr/scenario_engine/scenario_engine.hpp"
#include "mr/prediction_engine/prediction_engine.hpp"
#include "mr/decision_engine/decision_engine.hpp"
#include "mr/telemetry/telemetry_hub.hpp"
#include "mr/brain_core/brain_version.hpp"
#include "mr/common/config.hpp"
#include "mr/market_types/quote.hpp"
#include <vector>
namespace mr {
class MarketCorePipeline {
public:
    MarketCorePipeline();
    void configure(const ConfigRegistry& config);
    void process_event(const MarketEvent& event);
    [[nodiscard]] std::vector<TradeIntent> pending_intents() const { return pending_; }
    std::vector<TradeIntent> drain_pending_intents();
    [[nodiscard]] TelemetryHub& telemetry() { return telemetry_; }
private:
    SystemClock clock_;
    Normalizer normalizer_;
    QualityEngine quality_;
    FeedFusionEngine fusion_;
    MarketBrain brain_;
    PerceptionEngineFacade perception_;
    StructureEngine structure_;
    MarketConceptsEngine concepts_;
    MicrostructureEngine micro_;
    ScenarioEngine scenarios_;
    PredictionEngine prediction_;
    DecisionEngine decision_;
    TelemetryHub telemetry_;
    IdGenerator intent_ids_;
    std::vector<TradeIntent> pending_;
    double stale_ms_{500};
};
}