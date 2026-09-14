#pragma once

#include "mr/market_concepts/regime_features.hpp"
#include <array>
#include <cstddef>

namespace mr {

/**
 * Normalized structure feature dims for concept similarity.
 * Stage 8 may calibrate feature_scales / prototypes from historical outcomes.
 */
inline constexpr std::size_t kConceptFeatureDims = 16;

struct ConceptWeightConfig {
    std::array<std::array<double, kConceptFeatureDims>, kConceptCount> prototypes{};
    std::array<double, kConceptFeatureDims> feature_scales{};

    /** Descriptive default prototypes — not entry triggers. */
    static ConceptWeightConfig defaults();
};

}  // namespace mr
