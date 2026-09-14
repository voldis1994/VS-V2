#pragma once
#include "mr/position_brain/position_types.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/market_concepts/regime_features.hpp"
namespace mr {
class PositionBrain {
public:
    PositionState open(const TradeIntent& intent, double fill, double qty);
    PositionDecision evaluate(PositionState& pos, const PriceDynamics& pd, const RegimeFeatures& rg);
    void update_excursions(PositionState& pos, double price);
private:
    IdGenerator ids_;
    double pnl(const PositionState& p, double price) const;
};
}