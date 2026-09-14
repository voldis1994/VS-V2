#pragma once
#include "mr/decision/trade_decision.hpp"
namespace mr {
struct DecisionState {
    TradeIntent last{};
    bool has_last{false};
};
}
