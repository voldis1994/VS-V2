#pragma once
#include "mr/market_types/candle.hpp"
namespace mr {
struct NormalizedCandle : Candle { Timestamp normalized_time{}; };
class CandleNormalizer {
public:
    NormalizedCandle normalize(const Candle& c, Timestamp ts);
};
}