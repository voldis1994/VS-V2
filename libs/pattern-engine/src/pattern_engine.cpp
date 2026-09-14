#include "mr/pattern_engine/pattern_engine.hpp"
namespace mr {
std::size_t PatternEngine::observe(const PriceDynamics& pd, const StructureFeatures& st) {
    auto v = encoder_.encode(pd, st); history_.push_back(v);
    if (history_.size() > 1000) history_.erase(history_.begin());
    return cluster_.assign(v);
}
PatternStats PatternEngine::stats() const {
    PatternStatistics ps; return ps.compute(history_);
}
}