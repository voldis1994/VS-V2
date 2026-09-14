#pragma once
#include "mr/capital/capital_order.hpp"
#include "mr/decision/trade_decision.hpp"
namespace mr {
class OrderBuilder {
public:
    [[nodiscard]] CapitalOrderRequest from_intent(const TradeIntent& intent, double quantity) const;
};
}
