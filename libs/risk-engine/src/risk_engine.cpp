#include "mr/risk_engine/risk_engine.hpp"
namespace mr {
SizingResult RiskEngine::size_position(const TradeIntent& intent, double balance, double price) {
    SizingResult r;
    if (emergency_stop) { r.reason = "EMERGENCY_STOP"; return r; }
    if (intent.decision != EntryDecision::EntryReady) { r.reason = "NOT_READY"; return r; }
    double risk_per_unit = std::abs(price - intent.stop_loss);
    if (risk_per_unit <= 0) { r.reason = "INVALID_STOP"; return r; }
    double risk_budget = balance * (limits_.max_drawdown_pct / 100.0);
    r.quantity = std::min(limits_.max_position_size, risk_budget / risk_per_unit);
    r.approved = r.quantity > 0; return r;
}
GuardResult RiskEngine::pre_trade_check(const TradeIntent& intent, double spread_cost) {
    GuardResult g;
    if (emergency_stop) { g.pass = false; g.reason = "EMERGENCY_STOP"; return g; }
    if (daily_pnl_ <= -limits_.max_daily_loss) { g.pass = false; g.reason = "DAILY_LOSS"; return g; }
    if (intent.expected_value <= spread_cost) { g.pass = false; g.reason = "NEGATIVE_EV"; return g; }
    return g;
}
GuardResult RiskEngine::monitor_position(const PositionState& pos, double daily_pnl) {
    daily_pnl_ = daily_pnl;
    GuardResult g;
    if (emergency_stop || daily_pnl <= -limits_.max_daily_loss) { g.pass = false; g.reason = "EMERGENCY"; }
    return g;
}
}