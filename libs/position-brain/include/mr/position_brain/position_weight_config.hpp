#pragma once

namespace mr {

/**
 * Position-management scales — Stage-8 calibratable.
 * Continuous HOLD/PROTECT/REDUCE/EXIT evidence — not single-candle triggers.
 */
struct PositionWeightConfig {
    // Thesis continuity vs degradation
    double w_continuation{1.0};
    double w_invalidation{1.0};
    double w_reversal{1.0};
    double w_thesis_quality{1.0};

    // Excursion / path quality
    double w_mfe{1.0};
    double w_mae{1.0};
    double w_peak_retention{1.0};

    // Soft saturators for continuous scores
    double continuation_scale{1.0};
    double degradation_scale{1.0};
    double protect_scale{1.0};
    double reduce_scale{1.0};
    double exit_scale{1.0};

    // Geometry management (fractions of entry thesis distances; not magic market cuts)
    double stop_tighten_frac{1.0};
    double reduce_fraction{0.5};

    // Soft dynamics context weight (not a one-candle exit gate)
    double w_dynamics{0.25};

    static PositionWeightConfig defaults();
};

}  // namespace mr
