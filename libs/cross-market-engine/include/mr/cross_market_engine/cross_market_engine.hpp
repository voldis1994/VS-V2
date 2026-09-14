#pragma once
#include "mr/cross_market_engine/cross_market_features.hpp"
#include "mr/feed_fusion/feed_fusion_engine.hpp"
#include <unordered_map>
namespace mr {
class CrossMarketEngine {
public:
    void update(InstrumentId inst, const FeedFusionEngine& fusion);
    [[nodiscard]] CrossMarketFeatures features(InstrumentId inst) const;
private:
    std::unordered_map<InstrumentId, CrossMarketFeatures> cache_;
};
}