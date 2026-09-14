#include "mr/pattern_engine/pattern_cluster.hpp"
#include "mr/pattern_engine/pattern_similarity.hpp"
namespace mr {
std::size_t PatternCluster::assign(const PatternVector& v, double threshold) {
    PatternSimilarity sim;
    for (std::size_t i=0;i<centroids_.size();++i)
        if (sim.cosine(v, centroids_[i]) >= threshold) return i;
    centroids_.push_back(v);
    return centroids_.size()-1;
}
}