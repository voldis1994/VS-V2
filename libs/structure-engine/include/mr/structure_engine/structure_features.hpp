#pragma once

#include "mr/market_types/timeframe.hpp"
#include <cstdint>

namespace mr {

/** Trend bias from multi-swing structure (not a trade trigger). */
enum class TrendBias : std::int8_t {
    Unknown = 0,
    Up = 1,
    Down = -1,
    Range = 2
};

/** Confirmed swing classification from successive pivots. */
enum class SwingLabel : std::uint8_t {
    None = 0,
    HH = 1,
    HL = 2,
    LH = 3,
    LL = 4
};

/**
 * Authoritative market-structure features.
 * Mutated only from Capital CLOSED 1m+ OHLC (structure authority).
 * Fields used by scenario/pattern consumers are preserved.
 */
struct StructureFeatures {
    /** +1 bullish swing sequence, -1 bearish, 0 neutral/range. */
    double swing_state{0};
    double range_width{0};
    double range_boundary_distance{0};
    double breakout_strength{0};
    double failed_breakout{0};
    double acceptance{0};
    double rejection{0};
    double pullback_depth{0};
    double continuation_pressure{0};
    double compression{0};
    double expansion{0};
    double reversal_candidate{0};
    double range_position{0.5};

    TrendBias trend_direction{TrendBias::Unknown};
    double trend_strength{0};
    std::uint32_t trend_age{0};
    double volatility{0};
    double structure_quality{0};
    double structural_invalidation{0};

    SwingLabel last_swing{SwingLabel::None};
    bool in_range{false};
    bool in_pullback{false};
    bool breakout_active{false};
    bool failed_breakout_up{false};
    bool failed_breakout_down{false};
    bool breakout_up{false};
    bool breakout_down{false};

    Timeframe authority_tf{Timeframe::Minute1};
    std::uint32_t bar_count{0};
    std::uint32_t swing_count{0};
    double last_swing_high{0};
    double last_swing_low{0};
    double structure_high{0};
    double structure_low{0};
};

}  // namespace mr
