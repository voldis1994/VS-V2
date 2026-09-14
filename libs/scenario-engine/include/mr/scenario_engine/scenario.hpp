#pragma once
#include "mr/scenario_engine/scenario_type.hpp"
namespace mr {
struct Scenario { ScenarioType type{ScenarioType::Range}; double confidence{0}; double target_move{0}; double invalidation{0}; };
}