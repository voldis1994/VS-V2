#pragma once
#include "mr/capital/capital_order.hpp"
#include "mr/decision_engine/decision_types.hpp"
namespace mr {
class OrderBuilder {
public:
    CapitalOrderRequest from_intent(const TradeIntent& intent, double quantity) const;
};
}