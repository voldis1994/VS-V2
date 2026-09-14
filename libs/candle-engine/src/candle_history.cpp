#include "mr/candle_engine/candle_history.hpp"
namespace mr {
void CandleHistory::add(const Candle& c) { if (c.status == CandleStatus::Closed) series_.push_closed(c); }
}