#include "mr/cross_market_engine/cross_market_engine.hpp"
namespace mr {
void CrossMarketEngine::update(InstrumentId inst, const FeedFusionEngine& fusion) {
    CrossMarketFeatures f;
    auto c = fusion.consensus(inst); auto d = fusion.divergence(inst); auto ll = fusion.lead_lag(inst);
    f.consensus = c.confidence; f.divergence = d.mean; f.lead_lag = ll.probability;
    f.correlation = 1.0 - std::min(1.0, d.mean / std::max(c.mid, 1e-9) * 1000);
    cache_[inst] = f;
}
CrossMarketFeatures CrossMarketEngine::features(InstrumentId inst) const {
    auto it = cache_.find(inst); return it == cache_.end() ? CrossMarketFeatures{} : it->second;
}
}