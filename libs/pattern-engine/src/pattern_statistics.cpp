#include "mr/pattern_engine/pattern_statistics.hpp"
#include "mr/pattern_engine/pattern_similarity.hpp"
namespace mr {
PatternStats PatternStatistics::compute(const std::vector<PatternVector>& patterns) const {
    PatternStats s; s.cluster_count = patterns.size();
    if (patterns.size() < 2) return s;
    PatternSimilarity sim; double sum=0; std::size_t n=0;
    for (std::size_t i=1;i<patterns.size();++i) { sum += sim.cosine(patterns[i-1], patterns[i]); n++; }
    s.mean_similarity = n ? sum/n : 0; return s;
}
}