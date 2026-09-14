#pragma once
#include "mr/market_types/candle_status.hpp"
#include "mr/common/id.hpp"
#include <cmath>
namespace mr {
struct Candle {
    InstrumentId instrument{kInvalidInstrument};
    Timestamp open_time{};
    double open{0}, high{0}, low{0}, close{0};
    std::uint32_t ticks{0};
    CandleStatus status{CandleStatus::Forming};
    [[nodiscard]] double body() const { return close - open; }
    [[nodiscard]] double range() const { return high - low; }
    [[nodiscard]] double body_pct() const {
        const double m = std::max(std::abs(open), 1e-9);
        return body() / m;
    }
};
}