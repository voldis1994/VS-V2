#pragma once

#include "mr/decision/decision_engine.hpp"
#include <vector>

namespace mr {

enum class RiskIntentType : std::uint8_t { None = 0, Entry = 1, Exit = 2, Reduce = 3 };

struct RiskIntent {
    TradeIntentId id{0};
    InstrumentId instrument{kInvalidInstrument};
    RiskIntentType type{RiskIntentType::None};
    Direction direction{Direction::Flat};
    double reference_price{0};
    double size_fraction{0};
    double max_risk_fraction{0};
    double confidence{0};
    std::string human_explanation;
    std::vector<std::string> reason_codes;
};

class RiskEngine {
public:
    explicit RiskEngine(IdGenerator& intent_ids);
    std::vector<RiskIntent> from_decision(const DecisionResult& decision, double mid_price,
                                          double account_risk_budget = 0.01);

private:
    IdGenerator& intent_ids_;
};

}  // namespace mr
