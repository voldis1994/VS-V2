#pragma once

#include "mr/brain/brain_engine.hpp"
#include <string>
#include <vector>

namespace mr {

enum class DecisionAction : std::uint8_t { Wait = 0, Buy = 1, Sell = 2, Block = 3 };

struct DecisionResult {
    InstrumentId instrument{kInvalidInstrument};
    DecisionAction action{DecisionAction::Wait};
    double buy_score{0};
    double sell_score{0};
    double confidence{0};
    std::string reason;
    std::vector<std::string> reason_codes;
};

class DecisionEngine {
public:
    DecisionResult evaluate(const BrainSnapshot& brain, double spread, double min_confidence = 0.55);
};

}  // namespace mr
