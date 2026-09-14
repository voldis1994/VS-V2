#pragma once
#include "mr/prediction_engine/prediction.hpp"
#include "mr/scenario_engine/scenario.hpp"
#include "mr/pattern_engine/pattern_engine.hpp"
#include "mr/common/id.hpp"
namespace mr {
class PredictionEngine {
public:
    Prediction predict(const Scenario& scenario, const PriceDynamics& pd, Direction dir);
};
}