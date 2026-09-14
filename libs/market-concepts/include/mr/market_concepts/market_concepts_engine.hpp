#pragma once

#include "mr/market_concepts/regime_features.hpp"
#include "mr/market_concepts/concept_weight_config.hpp"
#include "mr/market_concepts/regime_similarity.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/structure_engine/structure_features.hpp"
#include <array>
#include <utility>

namespace mr {

/**
 * Multi-label market concept engine.
 *
 * - No exclusive regime state machine.
 * - All 14 concepts scored simultaneously from normalized structure.
 * - No hardcoded confidence / trigger thresholds to pick a concept.
 * - StructureFeatures are the primary truth; concepts are derived context.
 * - Weights injectable for Stage 8 outcome calibration.
 * - Never emits BUY/SELL triggers.
 */
class MarketConceptsEngine {
public:
    MarketConceptsEngine() : weights_(ConceptWeightConfig::defaults()) {}
    explicit MarketConceptsEngine(ConceptWeightConfig weights)
        : weights_(std::move(weights)) {}

    void set_weights(ConceptWeightConfig weights) { weights_ = std::move(weights); }
    [[nodiscard]] const ConceptWeightConfig& weights() const { return weights_; }

    RegimeFeatures evaluate(const PriceDynamics& pd, const StructureFeatures& st);

    [[nodiscard]] static std::array<double, kConceptFeatureDims>
    normalize_structure(const StructureFeatures& st);

    [[nodiscard]] Regime previous_dominant() const { return previous_dominant_; }

private:
    ConceptWeightConfig weights_;
    RegimeSimilarity similarity_;
    Regime previous_dominant_{Regime::Unknown};
};

}  // namespace mr
