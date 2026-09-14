#pragma once
#include "mr/position_brain/position_types.hpp"
#include "mr/position_brain/position_weight_config.hpp"
#include "mr/prediction_engine/prediction.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/common/id.hpp"

namespace mr {

/**
 * PositionBrain — continuous HOLD / PROTECT / REDUCE / EXIT after entry.
 * Does not invent entries. Does not exit solely because one candle is against the position.
 */
class PositionBrain {
public:
    explicit PositionBrain(PositionWeightConfig cfg = PositionWeightConfig::defaults());

    void set_weight_config(PositionWeightConfig cfg);
    [[nodiscard]] const PositionWeightConfig& weight_config() const { return cfg_; }

    PositionState open(const TradeIntent& intent,
                       double fill_price,
                       double qty,
                       const SidePrediction& entry_thesis = {});

    void update_excursions(PositionState& pos, double price) const;

    PositionDecision evaluate(PositionState& pos,
                              const SidePrediction& side_now,
                              const PriceDynamics& pd = {}) const;

    /** Convenience: pick the open-side thesis from a DualPrediction. */
    [[nodiscard]] static SidePrediction side_for(const DualPrediction& dual, Direction direction);

private:
    PositionWeightConfig cfg_{};
    IdGenerator ids_{};

    [[nodiscard]] double pnl(const PositionState& p, double price) const;
};

}  // namespace mr
