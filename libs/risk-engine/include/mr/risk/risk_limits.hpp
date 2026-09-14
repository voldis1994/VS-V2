#pragma once
namespace mr {
struct RiskLimits {
    double max_position_size{1.0};
    double max_daily_loss{1000.0};
    double max_drawdown_pct{5.0};
    double max_spread{2.0};
    bool emergency_stop{false};
};
}
