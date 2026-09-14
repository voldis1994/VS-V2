#pragma once
#include "mr/pattern_engine/pattern_encoder.hpp"
#include "mr/pattern_engine/pattern_similarity.hpp"
#include "mr/pattern_engine/pattern_cluster.hpp"
#include "mr/pattern_engine/pattern_statistics.hpp"
namespace mr {
class PatternEngine {
public:
    std::size_t observe(const PriceDynamics& pd, const StructureFeatures& st);
    [[nodiscard]] PatternStats stats() const;
private:
    PatternEncoder encoder_; PatternSimilarity sim_; PatternCluster cluster_;
    std::vector<PatternVector> history_;
};
}