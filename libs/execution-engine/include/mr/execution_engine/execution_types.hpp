#pragma once
#include "mr/capital/capital_order.hpp"
#include "mr/decision/trade_decision.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace mr {

enum class ExecutionStatus : std::uint8_t {
    Idle = 0,
    Submitted = 1,
    Filled = 2,
    Rejected = 3,
    Retrying = 4
};

struct ExecutionReport {
    TradeIntentId intent_id{0};
    InstrumentId instrument{kInvalidInstrument};
    Direction direction{Direction::Flat};
    ExecutionStatus status{ExecutionStatus::Idle};
    double requested_quantity{0};
    double filled_quantity{0};
    double fill_price{0};
    std::string deal_id;
    std::uint32_t attempts{0};
    std::vector<std::string> reason_codes;
    std::string explanation;
    CapitalOrderResponse last_response{};
};

}  // namespace mr
