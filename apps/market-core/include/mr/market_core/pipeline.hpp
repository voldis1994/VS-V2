#pragma once
#include "mr/brain/market_brain.hpp"
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
#include "mr/risk/risk_engine.hpp"
#include "mr/telemetry/telemetry_hub.hpp"
#include "mr/brain/brain_version.hpp"
#include "mr/common/config.hpp"
#include "mr/market_types/quote.hpp"
#include "mr/market_types/market_clock.hpp"
#include <optional>
#include <vector>

namespace mr {

class MarketCorePipeline {
public:
    MarketCorePipeline();
    void configure(const ConfigRegistry& config);

    /** RAW QUOTE path — never updates structure authority. */
    void process_event(const MarketEvent& event);

    /**
     * Capital closed 1m+ OHLC authority path.
     * Structure/context updates ONLY here.
     */
    void process_authority_ohlc(const Candle& closed, Timeframe tf);

    /** Fail-closed risk requires real equity; no defaults. */
    void set_account_equity(double equity);
    void clear_account_equity();

    [[nodiscard]] std::vector<TradeIntent> pending_intents() const { return pending_; }
    std::vector<TradeIntent> drain_pending_intents();
    [[nodiscard]] TelemetryHub& telemetry() { return telemetry_; }
    [[nodiscard]] const StructureEngine& structure() const { return structure_; }
    [[nodiscard]] bool has_account_equity() const { return account_equity_.has_value(); }

private:
    void handle_clock_events(const std::vector<MarketClockEvent>& events,
                             const PriceDynamics& pd,
                             const ConsensusQuote& consensus,
                             InstrumentId instrument);
    void run_decision_and_risk(const StructureFeatures& st,
                               const PriceDynamics& pd,
                               const ConsensusQuote& consensus,
                               InstrumentId instrument);

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
    RiskEngine risk_;
    TelemetryHub telemetry_;
    IdGenerator intent_ids_;
    std::vector<TradeIntent> pending_;
    double stale_ms_{500};
    std::optional<double> account_equity_{};  // unset => risk fail-closed
};

}  // namespace mr
