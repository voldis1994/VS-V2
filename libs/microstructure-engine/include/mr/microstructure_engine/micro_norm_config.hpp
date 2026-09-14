#pragma once

#include <array>
#include <cstddef>

namespace mr {

/**
 * Normalization / lookback parameters for 10s microstructure evidence.
 *
 * Measurement scales and windows — not BUY/SELL or setup triggers.
 * Stage 8 may calibrate these from historical outcomes.
 */
struct MicroNormConfig {
    // Lookbacks (bars) — structural windows, not trading gates.
    std::size_t lookback_breakout{12};
    std::size_t lookback_vol_short{3};
    std::size_t lookback_vol_long{10};
    std::size_t lookback_pressure{6};

    // Soft saturators for continuous [0,1] mapping (Stage-8 calibratable).
    double momentum_scale{0.002};       // |Δclose|/mid soft scale
    double accel_scale{0.002};          // |Δmomentum| soft scale
    double reclaim_scale{1.0};          // mid-reclaim intensity gain
    double breakout_scale{1.0};         // penetration / width gain
    double failed_breakout_scale{1.0};  // reverse-of-penetration gain
    double compression_scale{1.0};      // max(0, 1 - atr_s/atr_l) gain
    double expansion_scale{1.0};        // max(0, atr_s/atr_l - 1) gain
    double continuation_scale{1.0};     // blend gain before soft01
    double exhaustion_scale{1.0};       // blend gain before soft01
    double wick_pressure_weight{0.15};  // body/wick pressure mix weight

    /**
     * Entry-timing evidence blend weights (descriptive, not a trade gate).
     * Order: strength, continuation, anti_exhaustion, |pressure|,
     *        acceptance, reclaim, anti_failed_breakout.
     */
    std::array<double, 7> entry_timing_weights{{
        0.25, 0.20, 0.15, 0.15, 0.10, 0.10, 0.05}};

    static MicroNormConfig defaults();
};

}  // namespace mr
