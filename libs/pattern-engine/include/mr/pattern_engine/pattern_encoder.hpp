#pragma once
#include "mr/pattern_engine/pattern_vector.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/structure_engine/structure_features.hpp"
namespace mr {
class PatternEncoder {
public:
    PatternVector encode(const PriceDynamics& pd, const StructureFeatures& st) const;
};
}