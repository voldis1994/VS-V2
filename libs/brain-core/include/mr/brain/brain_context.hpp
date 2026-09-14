#pragma once

#include "mr/candle_engine/candle_engine.hpp"
#include "mr/feed_fusion/consensus_quote.hpp"
#include "mr/structure_engine/structure_features.hpp"
#include "mr/market_concepts/regime_features.hpp"

namespace mr {

/**
 * Single authoritative brain context per instrument.
 * Structure/regime mutate only via authority CLOSED 1m+ OHLC.
 */
struct BrainContext {
    InstrumentId instrument{kInvalidInstrument};
    ConsensusQuote consensus{};
    CandleEngineState candles{};
    StructureFeatures structure{};
    RegimeFeatures regime{};
    bool has_structure_authority{false};
    Timestamp ts{};
};

}  // namespace mr
