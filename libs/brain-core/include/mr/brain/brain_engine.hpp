#pragma once

#include "mr/candle/candle_engine.hpp"
#include <string>
#include <unordered_map>

namespace mr {

struct BrainScores {
    double structure{0};
    double momentum{0};
    double pressure{0};
    double behavior{0};
    double impact{0};
    double composite{0};
    std::string bias{"NEUTRAL"};
};

struct BrainSnapshot {
    InstrumentId instrument{kInvalidInstrument};
    Timestamp timestamp{};
    BrainScores scores{};
    std::size_t bar_count{0};
};

class BrainEngine {
public:
    BrainSnapshot update(InstrumentId instrument, const std::vector<CandleBar>& bars, Timestamp ts);

private:
    std::unordered_map<InstrumentId, BrainSnapshot> latest_;
};

}  // namespace mr
