#pragma once
#include "mr/structure_engine/structure_features.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/market_types/candle.hpp"
#include "mr/market_types/timeframe.hpp"

namespace mr {

/**
 * Structure/context engine.
 * MUST NOT be driven by raw quotes or forming candles.
 * Authority = Capital closed 1m+ OHLC only.
 */
class StructureEngine {
public:
    /** Update structure from an authority closed candle (Capital 1m+). */
    void on_authority_close(const Candle& closed, Timeframe tf, const PriceDynamics& pd);

    [[nodiscard]] StructureFeatures snapshot() const { return features_; }
    [[nodiscard]] bool has_authority() const { return has_authority_; }
    [[nodiscard]] Timeframe authority_timeframe() const { return authority_tf_; }

private:
    StructureFeatures features_;
    double session_high_{0}, session_low_{0};
    bool has_authority_{false};
    Timeframe authority_tf_{Timeframe::Minute1};
};

}  // namespace mr
