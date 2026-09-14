#pragma once

#include "mr/market_concepts/regime_features.hpp"
#include "mr/market_concepts/regime_similarity.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/structure_engine/structure_features.hpp"

namespace mr {

/**
 * Market concepts / 14-regime context classifier.
 * Regimes describe structure context — they are NOT entry triggers.
 */
class MarketConceptsEngine {
public:
    RegimeFeatures evaluate(const PriceDynamics& pd, const StructureFeatures& st);

    [[nodiscard]] Regime previous() const { return previous_; }

private:
    RegimeSimilarity similarity_;
    Regime previous_{Regime::Unknown};
};

}  // namespace mr
