#pragma once
#include "mr/market_types/candle.hpp"
#include <vector>
namespace mr {
class OhlcSource {
public:
    void push(const Candle& c);
    [[nodiscard]] const std::vector<Candle>& history() const { return candles_; }
private:
    std::vector<Candle> candles_;
};
}