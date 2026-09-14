#pragma once
#include "mr/pattern_engine/pattern_vector.hpp"
#include <vector>
namespace mr {
class PatternCluster {
public:
    std::size_t assign(const PatternVector& v, double threshold = 0.85);
    [[nodiscard]] const std::vector<PatternVector>& centroids() const { return centroids_; }
private:
    std::vector<PatternVector> centroids_;
};
}