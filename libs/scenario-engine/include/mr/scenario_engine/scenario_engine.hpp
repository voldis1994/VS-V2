#pragma once
#include "mr/scenario_engine/scenario.hpp"
#include "mr/structure_engine/structure_features.hpp"
#include "mr/market_concepts/regime_features.hpp"
#include "mr/microstructure_engine/microstructure_features.hpp"
#include <vector>
namespace mr {
class ScenarioEngine {
public:
    std::vector<Scenario> evaluate(const StructureFeatures& st, const RegimeFeatures& rg, const MicrostructureFeatures& ms);
};
}