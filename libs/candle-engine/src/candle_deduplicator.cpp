#include "mr/candle_engine/candle_deduplicator.hpp"
namespace mr {
bool CandleDeduplicator::accept(const Candle& c) {
    if (c.open_time == last_open_) return false;
    last_open_ = c.open_time; return true;
}
}