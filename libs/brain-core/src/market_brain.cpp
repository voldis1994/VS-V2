#include "mr/brain_core/market_brain.hpp"
namespace mr {
CandleEngine& MarketBrain::candles(InstrumentId inst) { return candle_engines_[inst]; }
void MarketBrain::on_normalized(const NormalizedEvent& e, const ConsensusQuote& consensus) {
    auto& ce = candles(e.instrument);
    ce.on_event(e, consensus.mid);
    BrainContext ctx; ctx.instrument = e.instrument; ctx.consensus = consensus;
    ctx.candles = ce.state(); ctx.ts = e.normalized_timestamp;
    state_.update(ctx);
    BrainEvent ev; ev.type = BrainEventType::Quote; ev.ts = e.normalized_timestamp;
    ev.instrument = e.instrument; router_.publish(ev);
}
BrainSnapshot MarketBrain::snapshot() const {
    auto s = state_.latest(); s.id = snapshot_ids_.next; return s;
}
}