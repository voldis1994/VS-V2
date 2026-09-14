#pragma once

namespace mr {

/**
 * Relative decision saturator scales — not absolute BUY/SELL confidence gates.
 * Stage 8 may calibrate from historical decision outcomes.
 */
struct DecisionWeightConfig {
    double edge_scale{0.35};
    double conflict_scale{0.5};
    double weakness_scale{0.5};
    double cost_scale{1.0};

    double w_quality{1.0};
    double w_continuation{1.0};
    double weak_quality_weight{1.0};
    double weak_ev_weight{1.0};

    double stop_adverse_weight{1.0};
    double stop_vol_weight{1.0};
    double stop_invalidation_weight{1.0};
    double stop_distance_scale{1.0};
    double stop_move_frac{1.0};

    double target_expected_weight{1.0};
    double target_vol_weight{1.0};
    double target_distance_scale{1.0};
    double target_move_frac{1.0};

    static DecisionWeightConfig defaults();
};

}  // namespace mr
