#include "mr/market_core/brain_runtime.hpp"

namespace mr {

void BrainRuntime::observe(const BrainSnapshot& snapshot) {
    snapshots_[snapshot.instrument] = snapshot;
}

BrainSnapshot BrainRuntime::latest(InstrumentId instrument) const {
    auto it = snapshots_.find(instrument);
    return it != snapshots_.end() ? it->second : BrainSnapshot{};
}

std::vector<BrainSnapshot> BrainRuntime::all() const {
    std::vector<BrainSnapshot> out;
    for (const auto& [_, snap] : snapshots_) out.push_back(snap);
    return out;
}

void sync_brain_runtime(BrainRuntime& runtime, const MarketCorePipeline& pipeline, InstrumentId instrument) {
    runtime.observe(pipeline.latest_brain(instrument));
}

}  // namespace mr
