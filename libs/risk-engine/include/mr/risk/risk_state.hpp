#pragma once
#include "mr/risk/risk_limits.hpp"
namespace mr {
struct RiskState {
    double daily_pnl{0};
    double open_exposure{0};
    double drawdown_pct{0};
    RiskLimits limits{};
    bool trading_halted{false};
};
}
