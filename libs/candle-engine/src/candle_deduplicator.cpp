#include "mr/candle_engine/candle_deduplicator.hpp"
namespace mr {
bool CandleDeduplicator::accept(const Candle& c) {
    // Reject duplicates and out-of-order (non-monotonic open_time).
    if (has_last_) {
        if (c.open_time == last_open_) return false;
        if (c.open_time < last_open_) return false;
    }
    has_last_ = true;
    last_open_ = c.open_time;
    return true;
}
}