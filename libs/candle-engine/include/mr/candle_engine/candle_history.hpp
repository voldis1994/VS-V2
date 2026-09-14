#pragma once
#include "mr/candle_engine/candle_series.hpp"
namespace mr {
class CandleHistory {
public:
    void add(const Candle& c);
    [[nodiscard]] std::size_t size() const { return series_.closed().size(); }
    [[nodiscard]] const CandleSeries& series() const { return series_; }
private:
    CandleSeries series_;
};
}