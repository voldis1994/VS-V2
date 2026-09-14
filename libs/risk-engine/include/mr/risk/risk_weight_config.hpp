#pragma once

namespace mr {

/**
 * Risk scales / limits — Stage-8 calibratable.
 * Not BUY/SELL triggers. RiskEngine is the sole pre-trade veto.
 */
struct RiskWeightConfig {
    double risk_budget_frac{0.01};
    double max_position_notional_frac{0.25};
    double max_quantity{100.0};

    double max_gross_exposure_frac{1.0};
    double max_net_exposure_frac{0.5};
    double max_open_positions{5.0};

    double max_daily_loss_frac{0.05};

    double max_spread_frac{0.002};
    double spread_cost_scale{1.0};
    double max_quote_age_ms{2000.0};

    double duplicate_window_ms{2000.0};
    double min_net_ev_scale{1.0};

    static RiskWeightConfig defaults();
};

}  // namespace mr
