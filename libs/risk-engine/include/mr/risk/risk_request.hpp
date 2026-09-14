#pragma once
#include "mr/decision/trade_decision.hpp"

namespace mr {

struct RiskRequest {
    TradeIntent intent{};
    double mid_price{0};

    /** Required (>0). Missing => fail-closed. No invented defaults. */
    double account_equity{0};
    double account_risk_budget{0.0};  // if >0 overrides cfg.risk_budget_frac

    double spread_cost{0};
    double spread{0};
    double quote_age_ms{0};
    bool broker_healthy{true};
    bool data_fresh{true};

    double open_gross_notional{0};
    double open_net_notional{0};
    double open_position_count{0};
    double daily_pnl{0};

    bool duplicate_order{false};
};

}  // namespace mr
