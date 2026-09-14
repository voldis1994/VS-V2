#pragma once
#include "mr/market_types/candle.hpp"
#include <vector>
namespace mr {
class CandleGapHandler {
public:
    std::vector<Candle> detect_gaps(const Candle& prev, const Candle& next, std::uint64_t bucket_ns);
};
}