#pragma once
#include "mr/decision/trade_decision.hpp"
namespace mr {
struct RiskRequest {
    TradeIntent intent{};
    double mid_price{0};
    double account_equity{0};
    double account_risk_budget{0.01};
};
}
