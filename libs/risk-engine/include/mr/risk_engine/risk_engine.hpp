#pragma once
#include "mr/risk_engine/risk_types.hpp"
#include "mr/decision_engine/decision_types.hpp"
#include "mr/position_brain/position_types.hpp"
namespace mr {
class RiskEngine {
public:
    explicit RiskEngine(RiskLimits limits) : limits_(limits) {}
    SizingResult size_position(const TradeIntent& intent, double balance, double price);
    GuardResult pre_trade_check(const TradeIntent& intent, double spread_cost);
    GuardResult monitor_position(const PositionState& pos, double daily_pnl);
    bool emergency_stop{false};
private:
    RiskLimits limits_;
    double daily_pnl_{0};
};
}