#include "mr/decision/decision_engine.hpp"
#include "mr/common/clock.hpp"

namespace mr {

double DecisionEngine::compute_ev(double prob, double win, double loss, double cost) const {
    return prob * win - (1.0 - prob) * loss - cost;
}

Opportunity DecisionEngine::evaluate_opportunity(const Scenario& sc, const Prediction& pred,
                                                 double spread_cost, Direction dir) {
    Opportunity o;
    o.direction = dir;
    o.probability = pred.probability;
    o.spread_cost = spread_cost;
    o.expected_value = compute_ev(pred.probability, pred.expected_mfe, pred.expected_mae, spread_cost);
    if (o.expected_value > spread_cost && pred.probability > 0.55) {
        o.action = dir == Direction::Long ? TradeAction::Buy : TradeAction::Sell;
    } else {
        o.action = TradeAction::Wait;
        o.reason_codes.push_back("LOW_EV");
    }
    if (sc.confidence < 0.3) {
        o.action = TradeAction::Wait;
        o.reason_codes.push_back("LOW_SCENARIO_CONF");
    }
    return o;
}

SideEvaluation DecisionEngine::evaluate_long_short_wait(const Scenario& sc,
                                                        PredictionEngine& prediction,
                                                        const PriceDynamics& pd,
                                                        double spread_cost) {
    SideEvaluation eval;
    auto pred_long = prediction.predict(sc, pd, Direction::Long);
    auto pred_short = prediction.predict(sc, pd, Direction::Short);
    eval.long_opp = evaluate_opportunity(sc, pred_long, spread_cost, Direction::Long);
    eval.short_opp = evaluate_opportunity(sc, pred_short, spread_cost, Direction::Short);

    const bool long_ok = eval.long_opp.action == TradeAction::Buy;
    const bool short_ok = eval.short_opp.action == TradeAction::Sell;

    if (long_ok && short_ok) {
        if (eval.long_opp.expected_value > eval.short_opp.expected_value) {
            eval.chosen = eval.long_opp;
            eval.final_action = TradeAction::Buy;
        } else if (eval.short_opp.expected_value > eval.long_opp.expected_value) {
            eval.chosen = eval.short_opp;
            eval.final_action = TradeAction::Sell;
        } else {
            eval.chosen = eval.long_opp;
            eval.chosen.action = TradeAction::Wait;
            eval.chosen.direction = Direction::Flat;
            eval.chosen.reason_codes.push_back("LONG_SHORT_TIE_WAIT");
            eval.final_action = TradeAction::Wait;
        }
    } else if (long_ok) {
        eval.chosen = eval.long_opp;
        eval.final_action = TradeAction::Buy;
    } else if (short_ok) {
        eval.chosen = eval.short_opp;
        eval.final_action = TradeAction::Sell;
    } else {
        eval.chosen = eval.long_opp.expected_value >= eval.short_opp.expected_value
            ? eval.long_opp : eval.short_opp;
        eval.chosen.action = TradeAction::Wait;
        eval.chosen.direction = Direction::Flat;
        eval.chosen.reason_codes.push_back("NO_SIDE_QUALIFIES_WAIT");
        eval.final_action = TradeAction::Wait;
    }
    return eval;
}

TradeIntent DecisionEngine::decide(const Opportunity& opp, const Quote& quote, std::uint64_t ttl_ms) {
    TradeIntent t;
    t.id = ids_.generate();
    t.instrument = opp.instrument;
    t.direction = opp.direction;
    t.created_at = now_utc_ns();
    t.expires_at = Timestamp(t.created_at.count() + static_cast<long long>(ttl_ms) * 1'000'000LL);
    t.probability = opp.probability;
    t.expected_value = opp.expected_value;
    if (!quote.valid) {
        t.decision = EntryDecision::Reject;
        t.reason_codes.push_back("NO_QUOTE");
        return t;
    }
    t.reference_price = quote.spread.mid_price();
    if (opp.action == TradeAction::Buy) {
        t.decision = EntryDecision::EntryReady;
        t.direction = Direction::Long;
        t.stop_loss = t.reference_price * 0.998;
        t.take_profit = t.reference_price * 1.004;
        t.explanation = "BUY opportunity";
    } else if (opp.action == TradeAction::Sell) {
        t.decision = EntryDecision::EntryReady;
        t.direction = Direction::Short;
        t.stop_loss = t.reference_price * 1.002;
        t.take_profit = t.reference_price * 0.996;
        t.explanation = "SELL opportunity";
    } else {
        t.decision = EntryDecision::NoTrade;
        t.direction = Direction::Flat;
        t.explanation = "WAIT";
        t.reason_codes = opp.reason_codes;
    }
    return t;
}

}  // namespace mr
