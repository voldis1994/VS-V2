#pragma once
#include "mr/market_types/spread.hpp"
#include "mr/common/id.hpp"
namespace mr {
struct Quote {
    InstrumentId instrument{kInvalidInstrument};
    SourceId source{kInvalidSource};
    Spread spread{};
    double last{0};
    Timestamp exchange_ts{};
    Timestamp receive_ts{};
    bool valid{false};
};
}