#pragma once
#include "mr/decision/opportunity.hpp"
#include "mr/decision/trade_decision.hpp"
#include "mr/prediction_engine/prediction.hpp"
#include "mr/prediction_engine/prediction_engine.hpp"
#include "mr/scenario_engine/scenario.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/market_types/quote.hpp"

namespace mr {

struct SideEvaluation {
    Opportunity long_opp{};
    Opportunity short_opp{};
    Opportunity chosen{};
    TradeAction final_action{TradeAction::Wait};
};

class DecisionEngine {
public:
    explicit DecisionEngine(IdGenerator& ids) : ids_(ids) {}

    Opportunity evaluate_opportunity(const Scenario& sc, const Prediction& pred,
                                     double spread_cost, Direction dir);

    /** Always evaluate LONG and SHORT; emit WAIT unless one side clearly wins. */
    SideEvaluation evaluate_long_short_wait(const Scenario& sc,
                                            PredictionEngine& prediction,
                                            const PriceDynamics& pd,
                                            double spread_cost);

    TradeIntent decide(const Opportunity& opp, const Quote& quote, std::uint64_t ttl_ms = 2000);

private:
    IdGenerator& ids_;
    double compute_ev(double prob, double win, double loss, double cost) const;
};

}  // namespace mr
