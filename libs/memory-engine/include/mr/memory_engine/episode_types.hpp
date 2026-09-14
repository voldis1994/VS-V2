#pragma once

#include "mr/brain/brain_version.hpp"
#include "mr/common/id.hpp"
#include "mr/decision/opportunity.hpp"
#include "mr/decision/trade_action.hpp"
#include "mr/execution_engine/execution_types.hpp"
#include "mr/market_concepts/regime_features.hpp"
#include "mr/market_types/candle.hpp"
#include "mr/market_types/market_event.hpp"
#include "mr/market_types/timeframe.hpp"
#include "mr/microstructure_engine/microstructure_features.hpp"
#include "mr/position_brain/position_types.hpp"
#include "mr/prediction_engine/prediction.hpp"
#include "mr/risk/risk_decision.hpp"
#include "mr/structure_engine/structure_features.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mr {

enum class EpisodeClockDomain : std::uint8_t {
    RawQuote = 0,
    ClosedTenSecond = 1,
    AuthorityOhlc = 2
};

struct EpisodeProvenance {
    std::string episode_schema_version{"vs-v2-episode-1"};
    std::string brain_version{kBrainVersion};
    std::string model_id{"default"};
    std::string config_hash;
    std::string prediction_weights_hash;
    std::string decision_weights_hash;
    std::string risk_weights_hash;
    std::string execution_weights_hash;
    std::string position_weights_hash;
};

struct EpisodeMarketItem {
    EpisodeClockDomain domain{EpisodeClockDomain::RawQuote};
    Timestamp ts{};
    std::uint64_t sequence{0};
    InstrumentId instrument{kInvalidInstrument};
    std::optional<MarketEvent> quote{};
    std::optional<Candle> candle{};
    Timeframe timeframe{Timeframe::Second1};
    bool structure_authority{false};
    bool one_shot{false};
};

struct EpisodeBrainFrame {
    Timestamp ts{};
    EpisodeClockDomain trigger{EpisodeClockDomain::RawQuote};
    InstrumentId instrument{kInvalidInstrument};
    StructureFeatures structure{};
    RegimeFeatures regime{};
    MicrostructureFeatures micro{};
    DualPrediction prediction{};
    Opportunity decision{};
    TradeAction decision_action{TradeAction::Wait};
    RiskDecision risk{};
    ExecutionReport execution{};
    PositionState position{};
    PositionDecision position_decision{};
    bool has_structure_authority{false};
    bool has_micro_authority{false};
    bool has_prediction{false};
    bool has_decision{false};
    bool has_risk{false};
    bool has_execution{false};
    bool has_position{false};
};

struct EpisodeOutcome {
    bool sealed{false};
    Direction direction{Direction::Flat};
    double entry_price{0};
    double exit_price{0};
    double quantity{0};
    double realized_pnl{0};
    double mfe{0};
    double mae{0};
    double peak_retention{0};
    PositionAction exit_action{PositionAction::Exit};
    ExitReason exit_reason{ExitReason::None};
    Timestamp entry_ts{};
    Timestamp exit_ts{};
    double post_exit_favorable{0};
    double post_exit_adverse{0};
    Timestamp post_exit_end_ts{};
    std::uint32_t post_exit_samples{0};
};

struct TradeEpisode {
    std::string episode_id;
    InstrumentId instrument{kInvalidInstrument};
    EpisodeProvenance provenance{};
    std::vector<EpisodeMarketItem> market;
    std::vector<EpisodeBrainFrame> frames;
    std::vector<PositionDecision> position_actions;
    EpisodeOutcome outcome{};
    bool sealed{false};
};

}  // namespace mr
