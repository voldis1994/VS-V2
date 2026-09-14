#pragma once

#include "mr/decision/opportunity.hpp"
#include "mr/decision/trade_decision.hpp"
#include "mr/decision/decision_weight_config.hpp"
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
    double buy_score{0};
    double sell_score{0};
    double wait_score{0};
};

/**
 * DecisionEngine — sole BUY / SELL / WAIT source.
 *
 * Chooses from relative LONG vs SHORT quality/EV and market evidence.
 * No hardcoded confidence BUY/SELL thresholds.
 * RAW quote used only in decide() for execution/safety (valid mid).
 * Prediction ≠ order.
 */
class DecisionEngine {
public:
    explicit DecisionEngine(IdGenerator& ids,
                            DecisionWeightConfig cfg = DecisionWeightConfig::defaults());

    void set_weight_config(DecisionWeightConfig cfg);
    [[nodiscard]] const DecisionWeightConfig& weight_config() const { return cfg_; }

    /** Stage-5 authority path: dual prediction → relative BUY/SELL/WAIT. */
    [[nodiscard]] SideEvaluation evaluate(const DualPrediction& dual,
                                          double spread_cost,
                                          InstrumentId instrument = kInvalidInstrument) const;

    /**
     * Legacy scenario path — still evaluates LONG and SHORT independently,
     * then routes through DualPrediction-style relative choice (no absolute gates).
     */
    SideEvaluation evaluate_long_short_wait(const Scenario& sc,
                                            PredictionEngine& prediction,
                                            const PriceDynamics& pd,
                                            double spread_cost);

    /** Quote is execution/safety only — never structural prediction input. */
    TradeIntent decide(const Opportunity& opp, const Quote& quote, std::uint64_t ttl_ms = 2000);

private:
    Opportunity from_side(const SidePrediction& side,
                          double spread_cost,
                          InstrumentId instrument) const;

    void apply_geometry(Opportunity& opp,
                        const SidePrediction& side,
                        const DualPrediction& dual) const;

    IdGenerator& ids_;
    DecisionWeightConfig cfg_{};
};

}  // namespace mr
