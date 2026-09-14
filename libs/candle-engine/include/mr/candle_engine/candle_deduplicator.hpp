#pragma once
#include "mr/market_types/candle.hpp"
namespace mr {
class CandleDeduplicator {
public:
    bool accept(const Candle& c);
private:
    bool has_last_{false};
    Timestamp last_open_{};
};
}