#pragma once
#include "mr/execution_engine/order_gateway.hpp"
#include "mr/execution_engine/order_builder.hpp"
#include "mr/execution_engine/fill_tracker.hpp"
#include "mr/execution_engine/execution_weight_config.hpp"
#include "mr/execution_engine/execution_types.hpp"
#include "mr/decision/trade_decision.hpp"
#include "mr/position_brain/position_types.hpp"
#include <unordered_map>

namespace mr {

/**
 * ExecutionEngine — Capital BUY/SELL order lifecycle only.
 * Never chooses trades. Consumes DecisionEngine intents already risk-approved.
 * EXIT/REDUCE management also routes here — no thesis invention.
 */
class ExecutionEngine {
public:
    explicit ExecutionEngine(OrderGateway& gateway,
                             ExecutionWeightConfig cfg = ExecutionWeightConfig::defaults());

    void set_weight_config(ExecutionWeightConfig cfg);
    [[nodiscard]] const ExecutionWeightConfig& weight_config() const { return cfg_; }

    /** Submit a risk-approved intent. Does not invent side or size policy. */
    ExecutionReport submit(const TradeIntent& intent, double quantity);

    /** Close via Capital deal id — management path, not a new thesis. */
    ExecutionReport close(const std::string& deal_id, TradeIntentId intent_id = 0);

    /**
     * Reduce open quantity via opposite-side Capital order — management path only.
     * Quantity/side come from PositionBrain output; this does not choose REDUCE.
     */
    ExecutionReport reduce(const PositionState& pos, double quantity, double price);

    [[nodiscard]] const FillTracker& fills() const { return fills_; }
    [[nodiscard]] bool was_recent_duplicate(InstrumentId instrument,
                                            Direction direction,
                                            Timestamp now) const;

private:
    OrderGateway& gateway_;
    ExecutionWeightConfig cfg_{};
    OrderBuilder builder_{};
    FillTracker fills_{};
    std::unordered_map<std::uint64_t, Timestamp> recent_submits_{};

    [[nodiscard]] static std::uint64_t dedup_key(InstrumentId instrument, Direction direction);
};

}  // namespace mr
