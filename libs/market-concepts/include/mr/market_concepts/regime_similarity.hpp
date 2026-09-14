#pragma once
#include "mr/market_concepts/regime_features.hpp"

namespace mr {

/** Compares full multi-concept score vectors (not exclusive labels). */
class RegimeSimilarity {
public:
    [[nodiscard]] double compare(const RegimeFeatures& a, const RegimeFeatures& b) const;
};

}  // namespace mr
