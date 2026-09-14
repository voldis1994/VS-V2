#pragma once
#include "mr/risk/risk_limits.hpp"
#include "mr/risk/risk_state.hpp"
#include "mr/risk/risk_request.hpp"
#include "mr/risk/risk_decision.hpp"
#include "mr/risk/exposure_state.hpp"
#include "mr/decision/trade_decision.hpp"
#include "mr/position_brain/position_types.hpp"
#include <string>

namespace mr {

struct SizingResult { double quantity{0}; bool approved{false}; std::string reason; };
struct GuardResult { bool pass{true}; std::string reason; };

class RiskEngine {
public:
    explicit RiskEngine(RiskLimits limits = {}) : limits_(limits) {}
    SizingResult size_position(const TradeIntent& intent, double balance, double price);
    GuardResult pre_trade_check(const TradeIntent& intent, double spread_cost);
    GuardResult monitor_position(const PositionState& pos, double daily_pnl);
    RiskDecision evaluate(const RiskRequest& request);
    bool emergency_stop{false};
private:
    RiskLimits limits_;
    double daily_pnl_{0};
};

}
