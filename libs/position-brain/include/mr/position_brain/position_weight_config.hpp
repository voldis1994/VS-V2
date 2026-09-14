#pragma once

namespace mr {

/**
 * Position-management scales — Stage-8 calibratable.
 * Continuous HOLD/PROTECT/REDUCE/EXIT evidence — not single-candle triggers.
 * All dynamics / MAE / MFE / protect / reduce mix weights live here (no baked-in market magics).
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

    // Soft dynamics / excursion saturators (Stage-8 calibratable)
    double dynamics_velocity_scale{1.0};
    double mae_scale{1.0};
    double mfe_scale{1.0};

    // Mix weights inside protect / reduce score assembly
    double protect_degradation_mix{0.5};
    double reduce_degradation_base{0.5};
    double reduce_mfe_mix{0.5};

    // Geometry management (fractions of entry thesis distances; not magic market cuts)
    double stop_tighten_frac{1.0};
    double reduce_fraction{0.5};

    // Soft dynamics context weight (not a one-candle exit gate)
    double w_dynamics{0.25};

    static PositionWeightConfig defaults();
};

}  // namespace mr
