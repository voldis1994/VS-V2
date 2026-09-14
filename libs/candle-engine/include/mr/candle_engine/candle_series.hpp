#pragma once
#include "mr/market_types/candle.hpp"
#include <deque>
namespace mr {
class CandleSeries {
public:
    void push_closed(const Candle& c);
    [[nodiscard]] const std::deque<Candle>& closed() const { return closed_; }
    [[nodiscard]] const Candle* last_closed() const { return closed_.empty() ? nullptr : &closed_.back(); }
private:
    std::deque<Candle> closed_;
};
}