#pragma once
#include "mr/candle_engine/candle_series.hpp"
#include "mr/market_types/timeframe.hpp"
#include <optional>
namespace mr {
class TimeframeAggregator {
public:
    explicit TimeframeAggregator(Timeframe target) : target_(target), bucket_ns_(timeframe_ns(target)) {}
    std::optional<Candle> on_closed_candle(const Candle& c);
private:
    Timeframe target_;
    std::uint64_t bucket_ns_;
    Candle forming_{};
    bool has_{false};
};
}