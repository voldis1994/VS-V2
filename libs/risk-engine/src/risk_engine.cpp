#include "mr/risk/risk_engine.hpp"
namespace mr {

SizingResult RiskEngine::size_position(const TradeIntent& intent, double balance, double price) {
    SizingResult r;
    if (emergency_stop || limits_.emergency_stop) { r.reason = "EMERGENCY_STOP"; return r; }
    if (intent.decision != EntryDecision::EntryReady) { r.reason = "NOT_READY"; return r; }
    double risk_per_unit = std::abs(price - intent.stop_loss);
    if (risk_per_unit <= 0) { r.reason = "INVALID_STOP"; return r; }
    double risk_budget = balance * (limits_.max_drawdown_pct / 100.0);
    r.quantity = std::min(limits_.max_position_size, risk_budget / risk_per_unit);
    r.approved = r.quantity > 0;
    return r;
}

GuardResult RiskEngine::pre_trade_check(const TradeIntent& intent, double spread_cost) {
    GuardResult g;
    if (emergency_stop || limits_.emergency_stop) { g.pass = false; g.reason = "EMERGENCY_STOP"; return g; }
    if (daily_pnl_ <= -limits_.max_daily_loss) { g.pass = false; g.reason = "DAILY_LOSS"; return g; }
    if (intent.expected_value <= spread_cost) { g.pass = false; g.reason = "NEGATIVE_EV"; return g; }
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

    auto guard = pre_trade_check(request.intent, 0.0);
    const double equity = request.account_equity > 0 ? request.account_equity : 10000.0;
    auto size = size_position(request.intent, equity, out.reference_price);
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

}
