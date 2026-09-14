#pragma once

#include "mr/candle_engine/candle_engine.hpp"
#include "mr/feed_fusion/consensus_quote.hpp"
#include "mr/structure_engine/structure_features.hpp"
#include "mr/market_concepts/regime_features.hpp"
#include "mr/microstructure_engine/microstructure_features.hpp"
#include "mr/prediction_engine/prediction.hpp"
#include "mr/decision/trade_action.hpp"
#include "mr/decision/opportunity.hpp"
#include "mr/risk/risk_decision.hpp"
#include "mr/execution_engine/execution_types.hpp"
#include "mr/position_brain/position_types.hpp"

namespace mr {

/**
 * Single authoritative brain context per instrument.
 * Structure/regime mutate only via Capital CLOSED 1m+ OHLC.
 * Microstructure mutates only via CLOSED 10s one-shot OHLC.
 * Prediction/decision/risk/execution/position are snapshots — one BrainState.
 */
struct BrainContext {
    InstrumentId instrument{kInvalidInstrument};
    ConsensusQuote consensus{};
    CandleEngineState candles{};
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
    Timestamp ts{};
};

}  // namespace mr
