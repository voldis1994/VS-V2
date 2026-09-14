#pragma once
#include "mr/brain/market_brain.hpp"
#include "mr/market_core/brain_runtime.hpp"
#include "mr/market_core/health_monitor.hpp"
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
#include "mr/execution_engine/execution_engine.hpp"
#include "mr/position_brain/position_brain.hpp"
#include "mr/telemetry/telemetry_hub.hpp"
#include "mr/brain/brain_version.hpp"
#include "mr/common/config.hpp"
#include "mr/market_types/quote.hpp"
#include "mr/market_types/market_clock.hpp"
#include "mr/memory_engine/episode_recorder.hpp"
#include "mr/common/id.hpp"
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace mr {

/**
 * Live path: DecisionEngine (sole BUY/SELL/WAIT) → RiskEngine (veto/size) →
 * ExecutionEngine (lifecycle) → Fill → PositionBrain (HOLD/PROTECT/REDUCE/EXIT).
 * EXIT/REDUCE management also routes through ExecutionEngine.
 * No second decision brain.
 */
class MarketCorePipeline {
public:
    MarketCorePipeline();
    void configure(const ConfigRegistry& config);

    /** Bind Capital order transport — enables live ExecutionEngine path. */
    void bind_order_gateway(OrderGateway& gateway);

    /**
     * Operating mode. Replay refuses Capital LIVE gateways.
     * Paper/Replay may use PaperOrderGateway only.
     */
    void set_operating_mode(OperatingMode mode);
    [[nodiscard]] OperatingMode operating_mode() const { return mode_; }

    /** Attach capture-only episode recorder (evidence/history — not a decision brain). */
    void attach_episode_recorder(EpisodeRecorder* recorder);
    [[nodiscard]] EpisodeRecorder* episode_recorder() const { return recorder_; }

    /** RAW QUOTE path — never updates structure authority. */
    void process_event(const MarketEvent& event);

    /**
     * Capital closed 1m+ OHLC authority path.
     * Structure/context updates ONLY here.
     */
    void process_authority_ohlc(const Candle& closed, Timeframe tf);

    /**
     * CLOSED 10s one-shot microstructure authority path.
     * Same production path as MarketClockKind::ClosedTenSecond from RAW quotes.
     * Used by EpisodeReplay to reinject recorded CLOSED 10s candles.
     * Never updates structure authority.
     */
    void process_closed_10s(const Candle& closed, Timestamp ts = {});

    /** Fail-closed risk requires real equity; no defaults. */
    void set_account_equity(double equity);
    void clear_account_equity();

    /** Restart recovery — load broker-open positions without inventing entries. */
    void hydrate_open_positions(std::vector<PositionState> recovered);

    /** Fail-closed broker probe for RiskEngine (false when no gateway / unhealthy). */
    void set_broker_healthy(bool healthy) { broker_healthy_ = healthy; }
    [[nodiscard]] bool broker_healthy() const;

    [[nodiscard]] std::vector<TradeIntent> pending_intents() const { return pending_; }
    std::vector<TradeIntent> drain_pending_intents();
    [[nodiscard]] TelemetryHub& telemetry() { return telemetry_; }
    [[nodiscard]] const StructureEngine& structure() const { return structure_; }
    [[nodiscard]] const MicrostructureEngine& micro() const { return micro_; }
    [[nodiscard]] BrainSnapshot brain_snapshot() const { return brain_.snapshot(); }
    [[nodiscard]] bool has_account_equity() const { return account_equity_.has_value(); }
    [[nodiscard]] bool has_execution() const { return static_cast<bool>(execution_); }
    [[nodiscard]] const std::vector<PositionState>& open_positions() const { return open_positions_; }
    [[nodiscard]] PositionBrain& position_brain() { return position_; }
    [[nodiscard]] ExecutionEngine* execution() { return execution_.get(); }
    [[nodiscard]] PredictionEngine& prediction_engine() { return prediction_; }
    [[nodiscard]] DecisionEngine& decision_engine() { return decision_; }
    [[nodiscard]] RiskEngine& risk_engine() { return risk_; }
    [[nodiscard]] BrainRuntime& brain_runtime() { return brain_runtime_; }
    [[nodiscard]] const BrainRuntime& brain_runtime() const { return brain_runtime_; }

    /** Push latest BrainSnapshot to Control API (throttled; no invented decisions). */
    void publish_brain_feed();

    /**
     * Continue after DecisionEngine produced EntryReady.
     * Path: Risk veto/size → Execution submit/fill → PositionBrain open → BrainState.
     * Does not invent BUY/SELL/WAIT.
     */
    bool enter_from_decision(const TradeIntent& intent,
                             const DualPrediction& dual,
                             double mid,
                             double spread);

    /**
     * Drive PositionBrain on open positions after market/Brain updates.
     * EXIT/REDUCE route through ExecutionEngine when a gateway is bound.
     */
    void update_open_positions(InstrumentId instrument,
                               const DualPrediction& dual,
                               const PriceDynamics& pd,
                               double mid);

private:
    void handle_clock_events(const std::vector<MarketClockEvent>& events,
                             const PriceDynamics& pd,
                             const ConsensusQuote& consensus,
                             InstrumentId instrument);
    void run_decision_and_risk(const StructureFeatures& st,
                               const PriceDynamics& pd,
                               const ConsensusQuote& consensus,
                               InstrumentId instrument);
    void execute_entry(const TradeIntent& intent,
                       const RiskDecision& risk,
                       const DualPrediction& dual,
                       Timestamp ts);
    void manage_open_positions(InstrumentId instrument,
                               const DualPrediction& dual,
                               const PriceDynamics& pd,
                               double mid,
                               Timestamp ts);
    void apply_position_action(PositionState& pos,
                               const PositionDecision& decision,
                               double mid,
                               Timestamp ts);

    SystemClock clock_;
    Normalizer normalizer_;
    QualityEngine quality_;
    FeedFusionEngine fusion_;
    MarketBrain brain_;
    BrainRuntime brain_runtime_;
    HealthMonitor health_;
    PerceptionEngineFacade perception_;
    StructureEngine structure_;
    MarketConceptsEngine concepts_;
    MicrostructureEngine micro_;
    ScenarioEngine scenarios_;
    PredictionEngine prediction_;
    DecisionEngine decision_;
    RiskEngine risk_;
    PositionBrain position_;
    std::unique_ptr<ExecutionEngine> execution_;
    TelemetryHub telemetry_;
    IdGenerator intent_ids_;
    std::vector<TradeIntent> pending_;
    std::vector<PositionState> open_positions_;
    std::unordered_map<InstrumentId, DualPrediction> last_dual_;
    double stale_ms_{500};
    std::optional<double> account_equity_{};  // unset => risk fail-closed
    bool broker_healthy_{false};  // fail-closed until LIVE gateway proves healthy
    OperatingMode mode_{OperatingMode::Live};
    EpisodeRecorder* recorder_{nullptr};
};

}  // namespace mr
