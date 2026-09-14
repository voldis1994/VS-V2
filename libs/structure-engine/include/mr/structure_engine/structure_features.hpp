#pragma once
namespace mr {
struct StructureFeatures {
    double swing_state{0}, range_width{0}, range_boundary_distance{0};
    double breakout_strength{0}, failed_breakout{0}, acceptance{0}, rejection{0};
    double pullback_depth{0}, continuation_pressure{0}, compression{0}, expansion{0};
    double reversal_candidate{0}, range_position{0.5};
};
}