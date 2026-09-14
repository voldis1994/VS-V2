#pragma once
#include "mr/market_types/candle.hpp"
namespace mr {
struct ClosedCandle : Candle { Timestamp close_time{}; };
}