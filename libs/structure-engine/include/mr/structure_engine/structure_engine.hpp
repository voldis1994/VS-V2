#pragma once
#include "mr/structure_engine/structure_features.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/candle_engine/candle_engine.hpp"
namespace mr {
class StructureEngine {
public:
    void update(double price, const PriceDynamics& pd, const CandleEngineState& candles);
    [[nodiscard]] StructureFeatures snapshot() const { return features_; }
private:
    StructureFeatures features_;
    double session_high_{0}, session_low_{0};
};
}