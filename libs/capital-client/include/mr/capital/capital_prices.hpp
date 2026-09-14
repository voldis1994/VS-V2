#pragma once
#include "mr/market_types/candle.hpp"
#include "mr/market_types/candle_status.hpp"
#include "mr/market_types/timeframe.hpp"
#include "mr/common/id.hpp"
#include <string>
#include <vector>

namespace mr {

struct CapitalPriceBar {
    Timestamp time{};
    double open{0}, high{0}, low{0}, close{0};
};

using CapitalPriceHistory = std::vector<CapitalPriceBar>;

/** Map Capital resolution string for closed OHLC (Minute1+ authority). */
inline std::string capital_resolution(Timeframe tf) {
    switch (tf) {
        case Timeframe::Minute1: return "MINUTE";
        case Timeframe::Minute5: return "MINUTE_5";
        case Timeframe::Minute15: return "MINUTE_15";
        case Timeframe::Hour1: return "HOUR";
        default: return "MINUTE";
    }
}

inline Candle to_closed_candle(const CapitalPriceBar& bar, InstrumentId instrument) {
    Candle c;
    c.instrument = instrument;
    c.open_time = bar.time;
    c.open = bar.open;
    c.high = bar.high;
    c.low = bar.low;
    c.close = bar.close;
    c.ticks = 1;
    c.status = CandleStatus::Closed;
    return c;
}

}  // namespace mr
