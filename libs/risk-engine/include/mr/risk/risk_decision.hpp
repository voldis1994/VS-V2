#pragma once
#include "mr/common/id.hpp"
#include <string>
#include <vector>
namespace mr {
enum class RiskIntentType : std::uint8_t { None = 0, Entry = 1, Exit = 2, Reduce = 3 };
struct RiskDecision {
    TradeIntentId id{0};
    InstrumentId instrument{kInvalidInstrument};
    RiskIntentType type{RiskIntentType::None};
    Direction direction{Direction::Flat};
    double reference_price{0};
    double size_fraction{0};
    double max_risk_fraction{0};
    double confidence{0};
    bool approved{false};
    std::string human_explanation;
    std::vector<std::string> reason_codes;
};
using RiskIntent = RiskDecision;
}
