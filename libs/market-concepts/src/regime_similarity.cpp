#include "mr/market_concepts/regime_similarity.hpp"
#include <cmath>
namespace mr {
double RegimeSimilarity::compare(const RegimeFeatures& a, const RegimeFeatures& b) const {
    if (a.current != b.current) return 0.2;
    return 1.0 - std::abs(a.confidence - b.confidence) - std::abs(a.trend_strength - b.trend_strength) * 0.5;
}
}