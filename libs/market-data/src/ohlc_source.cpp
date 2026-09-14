#include "mr/market_data/ohlc_source.hpp"
namespace mr {
void OhlcSource::push(const Candle& c) { candles_.push_back(c); }
}