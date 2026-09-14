#include "mr/candle_engine/candle_builder.hpp"
namespace mr {
std::uint64_t CandleBuilder::bucket_start(Timestamp ts) const {
    auto ns = static_cast<std::uint64_t>(ts.count());
    return (ns / bucket_ns_) * bucket_ns_;
}
CandleEvent CandleBuilder::on_tick(double price, Timestamp ts, InstrumentId inst) {
    CandleEvent ev; ev.ts = ts;
    auto bucket = bucket_start(ts);
    if (!has_forming_ || static_cast<std::uint64_t>(forming_.open_time.count()) != bucket) {
        if (has_forming_ && forming_.ticks > 0) {
            forming_.status = CandleStatus::Closed;
            ev.type = CandleEventType::Closed; ev.candle = forming_;
        }
        forming_ = {}; forming_.instrument = inst;
        forming_.open_time = Timestamp(static_cast<long long>(bucket));
        forming_.open = forming_.high = forming_.low = forming_.close = price;
        forming_.ticks = 1; forming_.status = CandleStatus::Forming;
        has_forming_ = true;
        if (ev.type != CandleEventType::Closed) ev.type = CandleEventType::Tick;
        ev.candle = forming_;
        return ev;
    }
    forming_.high = std::max(forming_.high, price);
    forming_.low = std::min(forming_.low, price);
    forming_.close = price; forming_.ticks++;
    ev.type = CandleEventType::Tick; ev.candle = forming_;
    return ev;
}
}