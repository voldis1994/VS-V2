#include "mr/prediction_engine/prediction_engine.hpp"
#include <cmath>
namespace mr {
Prediction PredictionEngine::predict(const Scenario& scenario, const PriceDynamics& pd, Direction dir) {
    Prediction p;
    p.probability = std::clamp(0.5 + scenario.confidence * 0.3, 0.05, 0.95);
    if (dir == Direction::Long && pd.directional_persistence < 0) p.probability -= 0.15;
    if (dir == Direction::Short && pd.directional_persistence > 0) p.probability -= 0.15;
    p.uncertainty = 1.0 - std::abs(pd.directional_persistence);
    p.expected_mfe = scenario.target_move;
    p.expected_mae = scenario.invalidation;
    p.expected_duration_s = 30.0 + scenario.confidence * 60.0;
    return p;
}
}