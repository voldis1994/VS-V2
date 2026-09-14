#pragma once
#include "mr/common/id.hpp"
#include <string>
namespace mr {
struct Instrument {
    InstrumentId id{kInvalidInstrument};
    std::string symbol;
    std::string epic;
    double tick_size{0.01};
    double lot_size{1.0};
};
}