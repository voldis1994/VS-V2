#include "mr/decision_engine/decision_engine.hpp"
#include "mr/common/clock.hpp"
namespace mr {
double DecisionEngine::compute_ev(double prob, double win, double loss, double cost) const {
    return prob * win - (1.0 - prob) * loss - cost;
}
Opportunity DecisionEngine::evaluate_opportunity(const Scenario& sc, const Prediction& pred, double spread_cost, Direction dir) {
    Opportunity o; o.direction = dir;
    o.probability = pred.probability;
    o.spread_cost = spread_cost;
    o.expected_value = compute_ev(pred.probability, pred.expected_mfe, pred.expected_mae, spread_cost);
    if (o.expected_value > spread_cost && pred.probability > 0.55) {
        o.action = dir == Direction::Long ? TradeAction::Buy : TradeAction::Sell;
    } else {
        o.action = TradeAction::Wait;
        o.reason_codes.push_back("LOW_EV");
    }
    if (sc.confidence < 0.3) { o.action = TradeAction::Wait; o.reason_codes.push_back("LOW_SCENARIO_CONF"); }
    return o;
}
TradeIntent DecisionEngine::decide(const Opportunity& opp, const Quote& quote, std::uint64_t ttl_ms) {
    TradeIntent t; t.id = ids_.generate(); t.instrument = opp.instrument; t.direction = opp.direction;
    t.created_at = now_utc_ns(); t.expires_at = Timestamp(t.created_at.count() + static_cast<long long>(ttl_ms) * 1'000'000LL);
    t.probability = opp.probability; t.expected_value = opp.expected_value;
    if (!quote.valid) { t.decision = EntryDecision::Reject; t.reason_codes.push_back("NO_QUOTE"); return t; }
    t.reference_price = quote.spread.mid_price();
    if (opp.action == TradeAction::Buy) {
        t.decision = EntryDecision::EntryReady;
        t.stop_loss = t.reference_price * 0.998; t.take_profit = t.reference_price * 1.004;
        t.explanation = "BUY opportunity";
    } else if (opp.action == TradeAction::Sell) {
        t.decision = EntryDecision::EntryReady;
        t.stop_loss = t.reference_price * 1.002; t.take_profit = t.reference_price * 0.996;
        t.explanation = "SELL opportunity";
    } else {
        t.decision = EntryDecision::NoTrade;
        t.explanation = "WAIT";
        t.reason_codes = opp.reason_codes;
    }
    return t;
}
}