#pragma once

#include "mr/candle_engine/candle_engine.hpp"
#include "mr/feed_fusion/consensus_quote.hpp"
#include "mr/structure_engine/structure_features.hpp"
#include "mr/market_concepts/regime_features.hpp"
#include "mr/microstructure_engine/microstructure_features.hpp"

namespace mr {

/**
 * Single authoritative brain context per instrument.
 * Structure/regime mutate only via Capital CLOSED 1m+ OHLC.
 * Microstructure mutates only via CLOSED 10s one-shot OHLC.
 */
struct BrainContext {
    InstrumentId instrument{kInvalidInstrument};
    ConsensusQuote consensus{};
    CandleEngineState candles{};
    StructureFeatures structure{};
    RegimeFeatures regime{};
    MicrostructureFeatures micro{};
    bool has_structure_authority{false};
    bool has_micro_authority{false};
    Timestamp ts{};
};

}  // namespace mr
