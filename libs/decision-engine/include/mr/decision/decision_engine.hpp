#pragma once
#include "mr/decision/opportunity.hpp"
#include "mr/decision/trade_decision.hpp"
#include "mr/decision/expected_value.hpp"
#include "mr/decision/action_score.hpp"
#include "mr/decision/decision_state.hpp"
#include "mr/prediction_engine/prediction.hpp"
#include "mr/scenario_engine/scenario.hpp"
#include "mr/market_types/quote.hpp"
namespace mr {
class DecisionEngine {
public:
    explicit DecisionEngine(IdGenerator& ids) : ids_(ids) {}
    Opportunity evaluate_opportunity(const Scenario& sc, const Prediction& pred, double spread_cost, Direction dir);
    TradeIntent decide(const Opportunity& opp, const Quote& quote, std::uint64_t ttl_ms = 2000);
private:
    IdGenerator& ids_;
    double compute_ev(double prob, double win, double loss, double cost) const;
};
}
