#include "mr/market_core/brain_runtime.hpp"

namespace mr {

void BrainRuntime::observe(const BrainSnapshot& snapshot) {
    latest_ = snapshot;
    has_ = true;
}

}  // namespace mr
