#pragma once
#include "mr/decision_engine/decision_types.hpp"
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