#pragma once

#include <array>
#include <cstddef>

namespace mr {

/**
 * Feature weights / saturator scales for dual-side prediction.
 * Measurement calibration — not BUY/SELL triggers.
 * Stage 8 may fit these from historical outcomes.
 */
struct PredictionWeightConfig {
    // Structure → side evidence weights
    double w_trend{1.0};
    double w_continuation{1.0};
    double w_pullback{0.7};
    double w_breakout{0.8};
    double w_failed_breakout{1.0};
    double w_reversal{1.0};
    double w_compression{0.4};
    double w_expansion{0.6};
    double w_structure_quality{0.8};
    double w_invalidation{1.0};

    // Microstructure → side evidence weights
    double w_micro_momentum{1.0};
    double w_micro_continuation{1.0};
    double w_micro_exhaustion{1.0};
    double w_micro_pressure{0.8};
    double w_micro_acceptance{0.6};
    double w_micro_rejection{0.8};
    double w_micro_reclaim{0.5};
    double w_micro_timing{0.7};
    double w_micro_failed_breakout{0.9};

    // Soft saturators for continuous [0,1] mapping
    double continuation_scale{1.0};
    double reversal_scale{1.0};
    double move_scale{1.0};
    double adverse_scale{1.0};
    double probability_scale{1.0};
    double confidence_scale{1.0};

    static PredictionWeightConfig defaults();
};

}  // namespace mr
