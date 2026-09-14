#pragma once
#include "mr/risk/risk_weight_config.hpp"
namespace mr {
/** Transitional POD — prefer RiskWeightConfig. */
struct RiskLimits {
    double max_position_size{1.0};
    double max_daily_loss{1000.0};
    double max_drawdown_pct{5.0};
    double max_spread{2.0};
    bool emergency_stop{false};

    [[nodiscard]] RiskWeightConfig to_weight_config() const {
        RiskWeightConfig c;
        c.max_quantity = max_position_size;
        if (max_daily_loss > 0.0) c.max_daily_loss_frac = max_daily_loss;  // absolute legacy
        return c;
    }
};
}
