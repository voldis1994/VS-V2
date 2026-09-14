#include "mr/risk/risk_engine.hpp"

namespace mr {

SizingResult RiskEngine::size_position(const TradeIntent& intent, double balance, double price) {
    SizingResult r;
    if (emergency_stop || limits_.emergency_stop) {
        r.reason = "EMERGENCY_STOP";
        return r;
    }
    if (!(balance > 0.0)) {
        r.reason = "MISSING_ACCOUNT_EQUITY";
        return r;
    }
    if (intent.decision != EntryDecision::EntryReady) {
        r.reason = "NOT_READY";
        return r;
    }
    const double risk_per_unit = std::abs(price - intent.stop_loss);
    if (risk_per_unit <= 0) {
        r.reason = "INVALID_STOP";
        return r;
    }
    const double risk_budget = balance * (limits_.max_drawdown_pct / 100.0);
    r.quantity = std::min(limits_.max_position_size, risk_budget / risk_per_unit);
    r.approved = r.quantity > 0;
    return r;
}

GuardResult RiskEngine::pre_trade_check(const TradeIntent& intent, double spread_cost) {
    GuardResult g;
    if (emergency_stop || limits_.emergency_stop) {
        g.pass = false;
        g.reason = "EMERGENCY_STOP";
        return g;
    }
    if (daily_pnl_ <= -limits_.max_daily_loss) {
        g.pass = false;
        g.reason = "DAILY_LOSS";
        return g;
    }
    if (intent.expected_value <= spread_cost) {
        g.pass = false;
        g.reason = "NEGATIVE_EV";
        return g;
    }
    return g;
}

GuardResult RiskEngine::monitor_position(const PositionState& pos, double daily_pnl) {
    (void)pos;
    daily_pnl_ = daily_pnl;
    GuardResult g;
    if (emergency_stop || limits_.emergency_stop || daily_pnl <= -limits_.max_daily_loss) {
        g.pass = false;
        g.reason = "EMERGENCY";
    }
    return g;
}

RiskDecision RiskEngine::evaluate(const RiskRequest& request) {
    RiskDecision out;
    out.id = request.intent.id;
    out.instrument = request.intent.instrument;
    out.direction = request.intent.direction;
    out.reference_price = request.mid_price > 0 ? request.mid_price : request.intent.reference_price;
    out.confidence = request.intent.probability;
    out.type = RiskIntentType::Entry;

    // Fail-closed: never invent equity / account defaults.
    if (!(request.account_equity > 0.0)) {
        out.approved = false;
        out.reason_codes.push_back("MISSING_ACCOUNT_EQUITY");
        out.human_explanation = "MISSING_ACCOUNT_EQUITY";
        return out;
    }
    if (!(out.reference_price > 0.0)) {
        out.approved = false;
        out.reason_codes.push_back("MISSING_REFERENCE_PRICE");
        out.human_explanation = "MISSING_REFERENCE_PRICE";
        return out;
    }

    auto guard = pre_trade_check(request.intent, 0.0);
    auto size = size_position(request.intent, request.account_equity, out.reference_price);
    out.size_fraction = size.quantity;
    out.max_risk_fraction = request.account_risk_budget;
    out.approved = guard.pass && size.approved;
    if (!out.approved) {
        out.reason_codes.push_back(guard.pass ? size.reason : guard.reason);
        out.human_explanation = out.reason_codes.empty() ? "rejected" : out.reason_codes.front();
    } else {
        out.human_explanation = "approved";
    }
    return out;
}

}  // namespace mr
