#pragma once

namespace mr {

/**
 * Relative decision saturator scales — not absolute BUY/SELL confidence gates.
 * Stage 8 may calibrate from historical decision outcomes.
 */
struct DecisionWeightConfig {
    double edge_scale{0.35};        // soft map of relative EV advantage
    double conflict_scale{0.5};     // soft map of opposing thesis clash
    double weakness_scale{0.5};     // soft map of insufficient thesis quality
    double cost_scale{1.0};         // how strongly spread cost reduces net EV

    // SL/TP placement as fractions of predicted move (execution geometry, not triggers)
    double stop_move_frac{1.0};
    double target_move_frac{1.0};

    static DecisionWeightConfig defaults();
};

}  // namespace mr
