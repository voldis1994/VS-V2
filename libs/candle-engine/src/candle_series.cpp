#include "mr/candle_engine/candle_series.hpp"
namespace mr {
void CandleSeries::push_closed(const Candle& c) { closed_.push_back(c); if (closed_.size() > 10000) closed_.pop_front(); }
}