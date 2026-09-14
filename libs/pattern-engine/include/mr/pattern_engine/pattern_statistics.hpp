#pragma once
#include "mr/pattern_engine/pattern_vector.hpp"
namespace mr {
struct PatternStats { double mean_similarity{0}; std::size_t cluster_count{0}; };
class PatternStatistics {
public:
    PatternStats compute(const std::vector<PatternVector>& patterns) const;
};
}