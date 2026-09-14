#include "mr/brain/market_brain.hpp"

namespace mr {

CandleEngine& MarketBrain::candles(InstrumentId inst) { return candle_engines_[inst]; }

std::vector<MarketClockEvent> MarketBrain::on_normalized(const NormalizedEvent& e,
                                                         const ConsensusQuote& consensus) {
    auto& ce = candles(e.instrument);
    auto clock_events = ce.on_event(e, consensus.mid);

    BrainContext ctx;
    ctx.instrument = e.instrument;
    ctx.consensus = consensus;
    ctx.candles = ce.state();
    ctx.ts = e.normalized_timestamp;
    state_.update(ctx);

    // Publish typed brain events — quote vs closed are distinct.
    BrainEvent quote_ev;
    quote_ev.type = BrainEventType::Quote;
    quote_ev.ts = e.normalized_timestamp;
    quote_ev.instrument = e.instrument;
    router_.publish(quote_ev);

    for (const auto& cev : clock_events) {
        if (cev.kind == MarketClockKind::ClosedTenSecond && cev.one_shot) {
            BrainEvent closed;
            closed.type = BrainEventType::CandleClosed;
            closed.ts = cev.ts;
            closed.instrument = cev.instrument;
            closed.payload = "CLOSED_10S";
            router_.publish(closed);
        } else if (cev.structure_authority) {
            BrainEvent closed;
            closed.type = BrainEventType::CandleClosed;
            closed.ts = cev.ts;
            closed.instrument = cev.instrument;
            closed.payload = "AUTHORITY_OHLC";
            router_.publish(closed);
        }
    }
    return clock_events;
}

BrainSnapshot MarketBrain::snapshot() const {
    auto s = state_.latest();
    s.id = snapshot_ids_.next;
    return s;
}

}  // namespace mr
