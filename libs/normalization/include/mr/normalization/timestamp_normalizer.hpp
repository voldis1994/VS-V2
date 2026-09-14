#pragma once
#include "mr/market_types/market_event.hpp"
namespace mr {
class TimestampNormalizer {
public:
    Timestamp normalize(const MarketEvent& e, Timestamp receive);
};
}