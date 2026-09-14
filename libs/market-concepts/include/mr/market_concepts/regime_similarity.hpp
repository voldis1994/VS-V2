#pragma once
#include "mr/market_concepts/regime_features.hpp"
namespace mr {
class RegimeSimilarity {
public:
    [[nodiscard]] double compare(const RegimeFeatures& a, const RegimeFeatures& b) const;
};
}