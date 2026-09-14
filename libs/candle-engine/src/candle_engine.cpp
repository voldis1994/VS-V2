#include "mr/candle_engine/candle_engine.hpp"
#include <algorithm>
namespace mr {
void CandleEngine::on_quote(double price, Timestamp ts, InstrumentId inst) {
    instrument_ = inst;
    auto ev = builder_10s_.on_tick(price, ts, inst);
    state_.forming_10s = builder_10s_.forming();
    state_.has_forming = builder_10s_.has_forming();
    auto ns = static_cast<std::uint64_t>(ts.count());
    auto bucket = (ns / timeframe_ns(Timeframe::Second10)) * timeframe_ns(Timeframe::Second10);
    state_.bucket_progress = std::clamp(static_cast<double>(ns - bucket) / timeframe_ns(Timeframe::Second10), 0.0, 1.0);
    if (ev.type == CandleEventType::Closed) {
        auto c = ev.candle; c.status = CandleStatus::Closed;
        if (dedup_.accept(c) && validator_.validate(c).ok) {
            ClosedCandle closed;
            static_cast<Candle&>(closed) = c;
            closed.close_time = ts;
            state_.last_closed_10s = closed;
            state_.has_closed = true;
            history_.add(c);
        }
    }
    builder_1s_.on_tick(price, ts, inst);
}
void CandleEngine::on_event(const NormalizedEvent& e, double consensus_mid) {
    double price = consensus_mid;
    if (price <= 0) {
        if (e.last) price = *e.last;
        else if (e.bid && e.ask) price = (*e.bid + *e.ask) * 0.5;
    }
    if (price > 0) on_quote(price, e.normalized_timestamp, e.instrument);
}
CandleEngineState CandleEngine::state() const { return state_; }
void CandleEngine::reset() { state_ = {}; history_ = CandleHistory{}; }
}