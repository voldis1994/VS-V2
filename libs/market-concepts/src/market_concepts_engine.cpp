#include "mr/market_concepts/market_concepts_engine.hpp"
#include <cmath>
namespace mr {
RegimeFeatures MarketConceptsEngine::evaluate(const PriceDynamics& pd, const StructureFeatures& st) {
    RegimeFeatures r;
    r.volatility = std::abs(pd.acceleration);
    r.trend_strength = std::abs(pd.directional_persistence);
    if (r.volatility > 0.5) { r.current = Regime::Volatile; r.confidence = 0.7; }
    else if (pd.directional_persistence > 0.5) { r.current = Regime::TrendUp; r.confidence = std::min(1.0, pd.directional_persistence); }
    else if (pd.directional_persistence < -0.5) { r.current = Regime::TrendDown; r.confidence = std::min(1.0, std::abs(pd.directional_persistence)); }
    else { r.current = Regime::Range; r.confidence = 1.0 - st.breakout_strength; }
    return r;
}
}