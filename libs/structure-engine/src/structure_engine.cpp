#include "mr/structure_engine/structure_engine.hpp"
#include <algorithm>
namespace mr {
void StructureEngine::update(double price, const PriceDynamics& pd, const CandleEngineState& candles) {
    if (session_high_ == 0 || price > session_high_) session_high_ = price;
    if (session_low_ == 0 || price < session_low_) session_low_ = price;
    features_.range_width = session_high_ - session_low_;
    features_.range_boundary_distance = std::min(price - session_low_, session_high_ - price);
    if (features_.range_width > 0) features_.range_position = (price - session_low_) / features_.range_width;
    features_.swing_state = pd.velocity > 0 ? 1 : (pd.velocity < 0 ? -1 : 0);
    if (candles.has_closed) {
        double bp = candles.last_closed_10s.body_pct();
        if (bp > 0.0004) { features_.continuation_pressure = std::min(1.0, std::abs(bp) * 200); features_.acceptance = features_.continuation_pressure; }
        else if (bp < -0.0004) { features_.rejection = std::min(1.0, std::abs(bp) * 200); }
        if (pd.short_horizon_momentum > 0 && bp < 0) features_.pullback_depth = std::min(1.0, std::abs(bp) * 250);
        else if (pd.short_horizon_momentum < 0 && bp > 0) features_.pullback_depth = std::min(1.0, std::abs(bp) * 250);
        features_.breakout_strength = std::abs(bp) > 0.001 ? std::min(1.0, std::abs(bp) * 100) : 0;
    }
    features_.reversal_candidate = std::abs(pd.directional_persistence) < 0.2 ? 0.5 : 0;
}
}