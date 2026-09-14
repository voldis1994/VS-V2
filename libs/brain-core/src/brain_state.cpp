#include "mr/brain/brain_state.hpp"

namespace mr {

void BrainState::update(const BrainContext& ctx) {
    BrainContext merged = ctx;
    const auto it = snapshot_.instruments.find(ctx.instrument);
    if (it != snapshot_.instruments.end()) {
        // RAW quote / forming / 10s must never wipe broad structure.
        if (!ctx.has_structure_authority) {
            merged.structure = it->second.structure;
            merged.regime = it->second.regime;
            merged.has_structure_authority = it->second.has_structure_authority;
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

}  // namespace mr
