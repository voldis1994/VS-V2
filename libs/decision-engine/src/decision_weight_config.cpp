#include "mr/decision/decision_weight_config.hpp"

namespace mr {

DecisionWeightConfig DecisionWeightConfig::defaults() {
    DecisionWeightConfig c;
    // Soft saturator scales for relative comparison — not absolute BUY/SELL gates.
    // Stage 8 may recalibrate from historical decision outcomes.
    c.edge_scale = 0.35;
    c.conflict_scale = 0.5;
    c.weakness_scale = 0.5;
    c.cost_scale = 1.0;
    c.stop_move_frac = 1.0;
    c.target_move_frac = 1.0;
    return c;
}

}  // namespace mr
