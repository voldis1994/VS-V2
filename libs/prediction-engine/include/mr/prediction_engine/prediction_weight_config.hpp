#pragma once

namespace mr {

/**
 * Feature weights / saturator scales for dual-side prediction.
 * Measurement calibration — not BUY/SELL triggers.
 * Stage 8 may fit these from historical outcomes.
 */
struct PredictionWeightConfig {
    double adverse_base_weight{1.0};
    double adverse_reject_weight{1.0};
    double adverse_scale{1.0};
    double adverse_stress_weight{1.0};
    double align_pressure_weight{1.0};
    double align_soft_scale{0.002};
    double confidence_scale{1.0};
    double continuation_scale{1.0};
    double failed_breakout_mismatch{0.5};
    double insufficient_confidence_scale{0.25};
    double inv_exhaustion_weight{1.0};
    double inv_reversal_weight{1.0};
    double inv_structure_weight{1.0};
    double legacy_duration_base_s{30.0};
    double legacy_duration_span_s{60.0};
    double legacy_momentum_scale{0.001};
    double legacy_pressure_long{0.6};
    double legacy_pressure_short{0.4};
    double momentum_soft_scale{0.002};
    double move_base_weight{1.0};
    double move_expansion_weight{1.0};
    double move_scale{1.0};
    double move_vol_weight{1.0};
    double probability_scale{1.0};
    double reversal_scale{1.0};
    double swing_couple_align{1.0};
    double swing_couple_base{1.0};
    double uncertainty_confidence_weight{1.0};
    double w_breakout{0.8};
    double w_compression{0.4};
    double w_continuation{1.0};
    double w_dynamics{1.0};
    double w_expansion{0.6};
    double w_failed_breakout{1.0};
    double w_invalidation{1.0};
    double w_micro_acceptance{0.6};
    double w_micro_continuation{1.0};
    double w_micro_exhaustion{1.0};
    double w_micro_failed_breakout{0.9};
    double w_micro_momentum{1.0};
    double w_micro_pressure{0.8};
    double w_micro_reclaim{0.5};
    double w_micro_rejection{0.8};
    double w_micro_timing{0.7};
    double w_pullback{0.7};
    double w_reversal{1.0};
    double w_structure_quality{0.8};
    double w_trend{1.0};

    static PredictionWeightConfig defaults();
};

}  // namespace mr
