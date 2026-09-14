#include "mr/candle_engine/candle_engine.hpp"
#include <algorithm>

namespace mr {

std::vector<MarketClockEvent> CandleEngine::on_quote(double price, Timestamp ts, InstrumentId inst) {
    std::vector<MarketClockEvent> out;
    instrument_ = inst;

    auto ev = builder_10s_.on_tick(price, ts, inst);
    state_.forming_10s = builder_10s_.forming();
    state_.has_forming = builder_10s_.has_forming();

    const auto ns = static_cast<std::uint64_t>(ts.count());
    const auto bucket = (ns / timeframe_ns(Timeframe::Second10)) * timeframe_ns(Timeframe::Second10);
    state_.bucket_progress = std::clamp(
        static_cast<double>(ns - bucket) / static_cast<double>(timeframe_ns(Timeframe::Second10)),
        0.0, 1.0);

    if (state_.has_forming) {
        MarketClockEvent forming;
        forming.kind = MarketClockKind::FormingCandle;
        forming.instrument = inst;
        forming.ts = ts;
        forming.timeframe = Timeframe::Second10;
        forming.candle = state_.forming_10s;
        forming.one_shot = false;
        forming.structure_authority = false;
        out.push_back(forming);
    }

    if (ev.type == CandleEventType::Closed) {
        auto c = ev.candle;
        c.status = CandleStatus::Closed;
        const bool fresh = !has_emitted_10s_ || c.open_time != last_emitted_10s_open_;
        if (fresh && dedup_10s_.accept(c) && validator_.validate(c).ok) {
            ClosedCandle closed;
            static_cast<Candle&>(closed) = c;
            closed.close_time = ts;
            state_.last_closed_10s = closed;
            state_.has_closed_10s = true;
            state_.has_closed = true;
            history_.add(c);
            last_emitted_10s_open_ = c.open_time;
            has_emitted_10s_ = true;

            MarketClockEvent closed_ev;
            closed_ev.kind = MarketClockKind::ClosedTenSecond;
            closed_ev.instrument = inst;
            closed_ev.ts = ts;
            closed_ev.timeframe = Timeframe::Second10;
            closed_ev.candle = c;
            closed_ev.one_shot = true;
            closed_ev.structure_authority = false;  // 10s is NOT structure authority
            out.push_back(closed_ev);

            if (auto m1 = agg_1m_.on_closed_candle(c)) {
                m1->status = CandleStatus::Closed;
                if (dedup_1m_.accept(*m1) && validator_.validate(*m1).ok) {
                    ClosedCandle c1;
                    static_cast<Candle&>(c1) = *m1;
                    c1.close_time = ts;
                    state_.last_closed_1m = c1;
                    state_.has_closed_1m = true;
                    history_.add(*m1);

                    MarketClockEvent m1_ev;
                    m1_ev.kind = MarketClockKind::ClosedOneMinute;
                    m1_ev.instrument = inst;
                    m1_ev.ts = ts;
                    m1_ev.timeframe = Timeframe::Minute1;
                    m1_ev.candle = *m1;
                    m1_ev.one_shot = true;
                    // Derived from quotes — not Capital authority.
                    m1_ev.structure_authority = false;
                    out.push_back(m1_ev);
                }
            }
        }
    }

    builder_1s_.on_tick(price, ts, inst);
    return out;
}

std::vector<MarketClockEvent> CandleEngine::on_event(const NormalizedEvent& e, double consensus_mid) {
    double price = consensus_mid;
    if (price <= 0) {
        if (e.last) price = *e.last;
        else if (e.bid && e.ask) price = (*e.bid + *e.ask) * 0.5;
    }
    if (price <= 0) return {};
    return on_quote(price, e.normalized_timestamp, e.instrument);
}

std::vector<MarketClockEvent> CandleEngine::ingest_authority_ohlc(const Candle& closed, Timeframe tf) {
    std::vector<MarketClockEvent> out;
    if (static_cast<std::uint32_t>(tf) < static_cast<std::uint32_t>(Timeframe::Minute1)) {
        return out;
    }
    Candle c = closed;
    c.status = CandleStatus::Closed;
    if (!dedup_authority_.accept(c) || !validator_.validate(c).ok) {
        return out;
    }

    ClosedCandle auth;
    static_cast<Candle&>(auth) = c;
    auth.close_time = Timestamp(c.open_time.count() + static_cast<long long>(timeframe_ns(tf)));
    state_.last_authority = auth;
    state_.has_authority = true;
    if (tf == Timeframe::Minute1) {
        state_.last_closed_1m = auth;
        state_.has_closed_1m = true;
    }
    history_.add(c);

    MarketClockEvent ev;
    ev.kind = (tf == Timeframe::Minute1)
        ? MarketClockKind::ClosedOneMinute
        : MarketClockKind::ClosedHigherTimeframe;
    ev.instrument = c.instrument;
    ev.ts = auth.close_time;
    ev.timeframe = tf;
    ev.candle = c;
    ev.one_shot = true;
    ev.structure_authority = true;  // Capital closed 1m+ ONLY
    out.push_back(ev);
    return out;
}

CandleEngineState CandleEngine::state() const { return state_; }

void CandleEngine::reset() {
    state_ = {};
    history_ = CandleHistory{};
    has_emitted_10s_ = false;
    last_emitted_10s_open_ = {};
}

}  // namespace mr
