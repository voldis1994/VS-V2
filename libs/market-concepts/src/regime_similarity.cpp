#include "mr/market_concepts/regime_similarity.hpp"
#include <algorithm>
#include <cmath>

namespace mr {

double RegimeSimilarity::compare(const RegimeFeatures& a, const RegimeFeatures& b) const {
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (std::size_t i = 0; i < kConceptCount; ++i) {
        dot += a.scores[i] * b.scores[i];
        na += a.scores[i] * a.scores[i];
        nb += b.scores[i] * b.scores[i];
    }
    const double denom = std::sqrt(na) * std::sqrt(nb);
    if (denom <= 1e-12) return 0.0;
    return std::clamp(dot / denom, 0.0, 1.0);
}

}  // namespace mr
