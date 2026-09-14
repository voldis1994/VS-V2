#include "mr/brain/brain_state.hpp"

namespace mr {

void BrainState::update(const BrainContext& ctx) {
    BrainContext merged = ctx;
    const auto it = snapshot_.instruments.find(ctx.instrument);
    if (it != snapshot_.instruments.end()) {
        if (!ctx.has_structure_authority) {
            merged.structure = it->second.structure;
            merged.regime = it->second.regime;
            merged.has_structure_authority = it->second.has_structure_authority;
        }
        if (!ctx.has_micro_authority) {
            merged.micro = it->second.micro;
            merged.has_micro_authority = it->second.has_micro_authority;
        }
        if (!ctx.has_prediction) {
            merged.prediction = it->second.prediction;
            merged.has_prediction = it->second.has_prediction;
        }
        if (!ctx.has_decision) {
            merged.decision = it->second.decision;
            merged.decision_action = it->second.decision_action;
            merged.has_decision = it->second.has_decision;
        }
        if (!ctx.has_risk) {
            merged.risk = it->second.risk;
            merged.has_risk = it->second.has_risk;
        }
        if (!ctx.has_execution) {
            merged.execution = it->second.execution;
            merged.has_execution = it->second.has_execution;
        }
        if (!ctx.has_position) {
            merged.position = it->second.position;
            merged.position_decision = it->second.position_decision;
            merged.has_position = it->second.has_position;
        }
    }
    snapshot_.instruments[ctx.instrument] = merged;
    snapshot_.ts = ctx.ts;
}

void BrainState::apply_authority(InstrumentId instrument,
                                 const StructureFeatures& structure,
                                 const RegimeFeatures& regime,
                                 Timestamp ts) {
    auto& ctx = snapshot_.instruments[instrument];
    ctx.instrument = instrument;
    ctx.structure = structure;
    ctx.regime = regime;
    ctx.has_structure_authority = true;
    ctx.ts = ts;
    snapshot_.ts = ts;
}

void BrainState::apply_micro(InstrumentId instrument,
                             const MicrostructureFeatures& micro,
                             Timestamp ts) {
    auto& ctx = snapshot_.instruments[instrument];
    ctx.instrument = instrument;
    ctx.micro = micro;
    ctx.has_micro_authority = true;
    ctx.ts = ts;
    snapshot_.ts = ts;
}

void BrainState::apply_prediction(InstrumentId instrument,
                                  const DualPrediction& prediction,
                                  Timestamp ts) {
    auto& ctx = snapshot_.instruments[instrument];
    ctx.instrument = instrument;
    ctx.prediction = prediction;
    ctx.has_prediction = true;
    ctx.ts = ts;
    snapshot_.ts = ts;
}

void BrainState::apply_decision(InstrumentId instrument,
                                const Opportunity& decision,
                                TradeAction action,
                                Timestamp ts) {
    auto& ctx = snapshot_.instruments[instrument];
    ctx.instrument = instrument;
    ctx.decision = decision;
    ctx.decision_action = action;
    ctx.has_decision = true;
    ctx.ts = ts;
    snapshot_.ts = ts;
}

void BrainState::apply_risk(InstrumentId instrument,
                            const RiskDecision& risk,
                            Timestamp ts) {
    auto& ctx = snapshot_.instruments[instrument];
    ctx.instrument = instrument;
    ctx.risk = risk;
    ctx.has_risk = true;
    ctx.ts = ts;
    snapshot_.ts = ts;
}

void BrainState::apply_execution(InstrumentId instrument,
                                 const ExecutionReport& execution,
                                 Timestamp ts) {
    auto& ctx = snapshot_.instruments[instrument];
    ctx.instrument = instrument;
    ctx.execution = execution;
    ctx.has_execution = true;
    ctx.ts = ts;
    snapshot_.ts = ts;
}

void BrainState::apply_position(InstrumentId instrument,
                                const PositionState& position,
                                const PositionDecision& decision,
                                Timestamp ts) {
    auto& ctx = snapshot_.instruments[instrument];
    ctx.instrument = instrument;
    ctx.position = position;
    ctx.position_decision = decision;
    ctx.has_position = true;
    ctx.ts = ts;
    snapshot_.ts = ts;
}

}  // namespace mr
