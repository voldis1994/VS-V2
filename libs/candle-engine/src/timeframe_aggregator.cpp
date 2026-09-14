#include "mr/candle_engine/timeframe_aggregator.hpp"
namespace mr {
std::optional<Candle> TimeframeAggregator::on_closed_candle(const Candle& c) {
    auto bucket = (static_cast<std::uint64_t>(c.open_time.count()) / bucket_ns_) * bucket_ns_;
    if (!has_ || static_cast<std::uint64_t>(forming_.open_time.count()) != bucket) {
        if (has_ && forming_.ticks > 0) {
            Candle out = forming_; forming_ = c; forming_.open_time = Timestamp(static_cast<long long>(bucket));
            has_ = true; return out;
        }
        forming_ = c; forming_.open_time = Timestamp(static_cast<long long>(bucket)); has_ = true; return std::nullopt;
    }
    forming_.high = std::max(forming_.high, c.high);
    forming_.low = std::min(forming_.low, c.low);
    forming_.close = c.close; forming_.ticks += c.ticks;
    return std::nullopt;
}
}