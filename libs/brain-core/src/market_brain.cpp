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
    // Authority flags stay false — update() preserves prior structure + micro.
    state_.update(ctx);

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

void MarketBrain::apply_authority_structure(InstrumentId instrument,
                                            const StructureFeatures& structure,
                                            const RegimeFeatures& regime,
                                            Timestamp ts) {
    state_.apply_authority(instrument, structure, regime, ts);

    BrainEvent closed;
    closed.type = BrainEventType::CandleClosed;
    closed.ts = ts;
    closed.instrument = instrument;
    closed.payload = "STRUCTURE_AUTHORITY";
    router_.publish(closed);
}

void MarketBrain::apply_micro_evidence(InstrumentId instrument,
                                       const MicrostructureFeatures& micro,
                                       Timestamp ts) {
    state_.apply_micro(instrument, micro, ts);

    BrainEvent closed;
    closed.type = BrainEventType::CandleClosed;
    closed.ts = ts;
    closed.instrument = instrument;
    closed.payload = "MICRO_10S_AUTHORITY";
    router_.publish(closed);
}

void MarketBrain::apply_prediction(InstrumentId instrument,
                                   const DualPrediction& prediction,
                                   Timestamp ts) {
    state_.apply_prediction(instrument, prediction, ts);
}

void MarketBrain::apply_decision(InstrumentId instrument,
                                 const Opportunity& decision,
                                 TradeAction action,
                                 Timestamp ts) {
    state_.apply_decision(instrument, decision, action, ts);
}

BrainSnapshot MarketBrain::snapshot() const {
    return state_.latest();
}

}  // namespace mr
