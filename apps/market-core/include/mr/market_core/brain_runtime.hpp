#pragma once

#include "mr/brain/brain_engine.hpp"
#include "mr/market_core/pipeline.hpp"
#include <unordered_map>

namespace mr {

/** Tracks per-instrument brain snapshots for streaming to control-api. */
class BrainRuntime {
public:
    void observe(const BrainSnapshot& snapshot);
    [[nodiscard]] BrainSnapshot latest(InstrumentId instrument) const;
    [[nodiscard]] std::vector<BrainSnapshot> all() const;

private:
    std::unordered_map<InstrumentId, BrainSnapshot> snapshots_;
};

void sync_brain_runtime(BrainRuntime& runtime, const MarketCorePipeline& pipeline, InstrumentId instrument);

}  // namespace mr
