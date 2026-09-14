#pragma once
#include "mr/market_types/candle.hpp"
namespace mr {
enum class CandleEventType : std::uint8_t { Tick=0, Closed=1, Gap=2 };
struct CandleEvent { CandleEventType type{CandleEventType::Tick}; Candle candle{}; Timestamp ts{}; };
}