#pragma once
#include "mr/brain_core/brain_state.hpp"
#include "mr/brain_core/brain_event_router.hpp"
#include "mr/candle_engine/candle_engine.hpp"
#include "mr/feed_fusion/feed_fusion_engine.hpp"
#include "mr/market_types/market_event.hpp"
namespace mr {
class MarketBrain {
public:
    void on_normalized(const NormalizedEvent& e, const ConsensusQuote& consensus);
    [[nodiscard]] BrainSnapshot snapshot() const;
    [[nodiscard]] CandleEngine& candles(InstrumentId inst);
    BrainEventRouter& router() { return router_; }
private:
    BrainState state_;
    BrainEventRouter router_;
    std::unordered_map<InstrumentId, CandleEngine> candle_engines_;
    IdGenerator snapshot_ids_;
};
}