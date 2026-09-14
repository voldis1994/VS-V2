#pragma once

#include "mr/brain/brain_snapshot.hpp"
#include "mr/brain/brain_context.hpp"
#include "mr/prediction_engine/prediction.hpp"
#include "mr/decision/opportunity.hpp"
#include "mr/decision/trade_action.hpp"

namespace mr {

/**
 * One authoritative BrainState.
 * Quote/forming updates must not overwrite structure/regime or closed-10s micro.
 */
class BrainState {
public:
    /** Quote-path update — preserves structure/regime/micro/prediction/decision authority. */
    void update(const BrainContext& ctx);

    /** Capital CLOSED 1m+ — mutates structure/regime; preserves micro. */
    void apply_authority(InstrumentId instrument,
                         const StructureFeatures& structure,
                         const RegimeFeatures& regime,
                         Timestamp ts);

    /** CLOSED 10s one-shot — mutates micro evidence; never rewrites structure. */
    void apply_micro(InstrumentId instrument,
                     const MicrostructureFeatures& micro,
                     Timestamp ts);

    /** Prediction snapshot into the same BrainState (not an order). */
    void apply_prediction(InstrumentId instrument,
                          const DualPrediction& prediction,
                          Timestamp ts);

    /** DecisionEngine choice snapshot — sole BUY/SELL/WAIT record. */
    void apply_decision(InstrumentId instrument,
                        const Opportunity& decision,
                        TradeAction action,
                        Timestamp ts);

    [[nodiscard]] const BrainSnapshot& latest() const { return snapshot_; }

private:
    BrainSnapshot snapshot_;
};

}  // namespace mr
