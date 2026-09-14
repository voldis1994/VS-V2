#pragma once
#include "mr/market_types/price.hpp"
namespace mr {
struct Spread { double bid{0}; double ask{0};
    [[nodiscard]] double width() const { return ask - bid; }
    [[nodiscard]] double mid_price() const { return mid(bid, ask); }
};
}