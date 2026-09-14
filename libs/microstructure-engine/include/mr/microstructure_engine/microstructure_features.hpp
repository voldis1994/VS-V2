#pragma once

#include "mr/market_types/timeframe.hpp"
#include <cstdint>

namespace mr {

/**
 * 10s microstructure evidence features.
 * Mutated authoritatively only from CLOSED 10s OHLC (one-shot).
 * Continuous descriptive scores — never BUY/SELL triggers.
 * Quote/book fields may update from RAW quotes but cannot confirm setup.
 */
struct MicrostructureFeatures {
    // --- quote/book (may refresh on RAW quote; not setup confirmation) ---
    double spread{0};
    double microprice{0};
    double bid_ask_imbalance{0};
    double aggressive_buy_pressure{0};
    double aggressive_sell_pressure{0};

    // --- CLOSED 10s candle geometry ---
    double body_pct{0};
    double upper_wick_pct{0};
    double lower_wick_pct{0};
    double candle_strength{0};

    // --- momentum ---
    double momentum{0};
    double acceleration{0};
    double deceleration{0};

    // --- rejection / acceptance / reclaim (OHLC evidence) ---
    double acceptance{0};
    double rejection{0};
    double reclaim{0};
    // Legacy aliases used by scenario consumers
    double rejection_proxy{0};
    double reclaim_proxy{0};
    double exhaustion_proxy{0};

    // --- local swing sequence ---
    double swing_state{0};  // +1 bullish / -1 bearish / 0 neutral
    double local_hh{0};
    double local_hl{0};
    double local_lh{0};
    double local_ll{0};
    double last_swing_high{0};
    double last_swing_low{0};
    std::uint32_t swing_count{0};

    // --- micro breakout ---
    double breakout_strength{0};
    double failed_breakout{0};
    bool breakout_up{false};
    bool breakout_down{false};
    bool failed_breakout_up{false};
    bool failed_breakout_down{false};

    // --- pullback ---
    double pullback_depth{0};
    double pullback_quality{0};

    // --- compression / expansion ---
    double compression{0};
    double expansion{0};
    double volatility{0};

    // --- buyer / seller pressure (from CLOSED 10s bodies/wicks) ---
    double buyer_pressure{0};
    double seller_pressure{0};
    double pressure_delta{0};

    // --- continuation vs exhaustion ---
    double continuation{0};
    double exhaustion{0};

    // --- entry timing quality (evidence score — not a trade trigger) ---
    double entry_timing_quality{0};

    /** True only after at least one CLOSED 10s one-shot authority update. */
    bool has_authority{false};
    /** True only when CLOSED 10s authority confirmed evidence (never from forming/RAW). */
    bool setup_confirmed{false};
    std::uint32_t closed_10s_count{0};
    Timeframe authority_tf{Timeframe::Second10};
};

}  // namespace mr
