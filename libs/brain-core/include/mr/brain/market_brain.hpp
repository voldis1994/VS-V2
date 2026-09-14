#pragma once

#include "mr/brain/brain_state.hpp"
#include "mr/brain/brain_event_router.hpp"
#include "mr/brain/brain_snapshot.hpp"
#include "mr/candle_engine/candle_engine.hpp"
#include "mr/feed_fusion/feed_fusion_engine.hpp"
#include "mr/structure_engine/structure_features.hpp"
#include "mr/market_concepts/regime_features.hpp"
#include "mr/microstructure_engine/microstructure_features.hpp"
#include "mr/market_types/market_event.hpp"
#include "mr/market_types/market_clock.hpp"
#include <unordered_map>
#include <vector>

namespace mr {

class MarketBrain {
public:
    /** Drive quote clock; returns forming / closed-10s / derived-1m clock events. */
    std::vector<MarketClockEvent> on_normalized(const NormalizedEvent& e,
                                                const ConsensusQuote& consensus);

    /** Authority CLOSED 1m+ — only path that mutates structure/regime in BrainState. */
    void apply_authority_structure(InstrumentId instrument,
                                   const StructureFeatures& structure,
                                   const RegimeFeatures& regime,
                                   Timestamp ts);

    /** CLOSED 10s one-shot — mutates micro evidence; never rewrites structure. */
    void apply_micro_evidence(InstrumentId instrument,
                              const MicrostructureFeatures& micro,
                              Timestamp ts);

    [[nodiscard]] BrainSnapshot snapshot() const;
    [[nodiscard]] CandleEngine& candles(InstrumentId inst);
    BrainEventRouter& router() { return router_; }

private:
    BrainState state_;
    BrainEventRouter router_;
    std::unordered_map<InstrumentId, CandleEngine> candle_engines_;
    IdGenerator snapshot_ids_;
};

}  // namespace mr
