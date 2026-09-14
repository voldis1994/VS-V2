#pragma once

#include "mr/common/id.hpp"

namespace mr {

/**
 * One-sided predictive thesis (LONG or SHORT).
 * Continuous evidence scores — never an order and never a BUY/SELL gate.
 */
struct SidePrediction {
    Direction direction{Direction::Flat};

    double continuation{0};       // continuation evidence [0,1]
    double reversal_failure{0};   // reversal / failure evidence [0,1]
    double expected_move{0};      // favorable move magnitude (normalized)
    double adverse_move{0};       // adverse move magnitude (normalized)
    double probability{0};        // soft success probability [0,1]
    double confidence{0};         // thesis confidence / certainty [0,1]
    double expected_value{0};     // p*move - (1-p)*adverse (pre-cost)
    double invalidation{0};       // thesis invalidation mass [0,1]
    double thesis_quality{0};     // overall thesis quality [0,1]
    double uncertainty{0};        // residual uncertainty [0,1]
};

/**
 * Independent LONG and SHORT predictions from one BrainState evidence view.
 * Prediction ≠ order. DecisionEngine is the sole BUY/SELL/WAIT source.
 */
struct DualPrediction {
    SidePrediction long_side{};
    SidePrediction short_side{};
    bool has_structure_authority{false};
    bool has_micro_authority{false};
    bool evidence_sufficient{false};
};

/** Legacy single-side prediction payload (scenario consumers / compat). */
struct Prediction {
    double probability{0.5};
    double uncertainty{0.5};
    double expected_mfe{0};
    double expected_mae{0};
    double expected_duration_s{0};
};

}  // namespace mr
