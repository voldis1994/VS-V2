#pragma once

#include "mr/brain/brain_snapshot.hpp"
#include "mr/brain/brain_context.hpp"

namespace mr {

/**
 * One authoritative BrainState.
 * Quote/forming/10s updates must not overwrite structure/regime.
 */
class BrainState {
public:
    /** Quote-path update — preserves prior structure/regime authority. */
    void update(const BrainContext& ctx);

    /** Authority-path mutation — structure/regime only change here. */
    void apply_authority(InstrumentId instrument,
                         const StructureFeatures& structure,
                         const RegimeFeatures& regime,
                         Timestamp ts);

    [[nodiscard]] const BrainSnapshot& latest() const { return snapshot_; }

private:
    BrainSnapshot snapshot_;
};

}  // namespace mr
