#pragma once

#include "mr/brain/brain_snapshot.hpp"
#include "mr/brain/brain_context.hpp"
#include "mr/prediction_engine/prediction.hpp"
#include "mr/decision/opportunity.hpp"
#include "mr/decision/trade_action.hpp"
#include "mr/risk/risk_decision.hpp"
#include "mr/execution_engine/execution_types.hpp"
#include "mr/position_brain/position_types.hpp"

namespace mr {

/**
 * One authoritative BrainState.
 * Quote/forming updates must not overwrite structure/regime or closed-10s micro.
 */
class BrainState {
public:
    /** Quote-path update — preserves authority snapshots. */
    void update(const BrainContext& ctx);

    void apply_authority(InstrumentId instrument,
                         const StructureFeatures& structure,
                         const RegimeFeatures& regime,
                         Timestamp ts);

    void apply_micro(InstrumentId instrument,
                     const MicrostructureFeatures& micro,
                     Timestamp ts);

    void apply_prediction(InstrumentId instrument,
                          const DualPrediction& prediction,
                          Timestamp ts);

    void apply_decision(InstrumentId instrument,
                        const Opportunity& decision,
                        TradeAction action,
                        Timestamp ts);

    /** RiskEngine veto/limit snapshot — never a BUY/SELL/WAIT choice. */
    void apply_risk(InstrumentId instrument,
                    const RiskDecision& risk,
                    Timestamp ts);

    /** ExecutionEngine lifecycle snapshot — never a trading thesis. */
    void apply_execution(InstrumentId instrument,
                         const ExecutionReport& execution,
                         Timestamp ts);

    /** PositionBrain management snapshot — HOLD/PROTECT/REDUCE/EXIT. */
    void apply_position(InstrumentId instrument,
                        const PositionState& position,
                        const PositionDecision& decision,
                        Timestamp ts);

    [[nodiscard]] const BrainSnapshot& latest() const { return snapshot_; }

private:
    BrainSnapshot snapshot_;
};

}  // namespace mr
