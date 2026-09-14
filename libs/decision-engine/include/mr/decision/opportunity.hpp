#pragma once
#include "mr/decision/trade_action.hpp"
#include "mr/decision/decision_reason.hpp"
#include "mr/common/id.hpp"
namespace mr {
struct Opportunity {
    InstrumentId instrument{kInvalidInstrument};
    Direction direction{Direction::Flat};
    double probability{0};
    double expected_value{0};
    double spread_cost{0};
    TradeAction action{TradeAction::Wait};
    DecisionReasonCodes reason_codes;
};
}
