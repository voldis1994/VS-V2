#pragma once

#include "mr/brain/market_brain.hpp"
#include "mr/market_core/pipeline.hpp"
#include <vector>

namespace mr {

/** Tracks brain snapshots for streaming to control-api. */
class BrainRuntime {
public:
    void observe(const BrainSnapshot& snapshot);
    [[nodiscard]] BrainSnapshot latest() const { return latest_; }
    [[nodiscard]] bool has_snapshot() const { return has_; }

private:
    BrainSnapshot latest_{};
    bool has_{false};
};

inline void sync_brain_runtime(BrainRuntime& runtime, MarketBrain& brain) {
    runtime.observe(brain.snapshot());
}

}  // namespace mr
