#pragma once
#include "mr/market_concepts/regime_features.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/structure_engine/structure_features.hpp"
namespace mr {
class MarketConceptsEngine {
public:
    RegimeFeatures evaluate(const PriceDynamics& pd, const StructureFeatures& st);
private:
    RegimeSimilarity similarity_;
};
}