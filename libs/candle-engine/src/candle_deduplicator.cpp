#include "mr/candle_engine/candle_deduplicator.hpp"
namespace mr {
bool CandleDeduplicator::accept(const Candle& c) {
    if (has_last_ && c.open_time == last_open_) return false;
    has_last_ = true;
    last_open_ = c.open_time;
    return true;
}
}