#include "mr/scenario_engine/scenario_engine.hpp"
namespace mr {
std::vector<Scenario> ScenarioEngine::evaluate(const StructureFeatures& st, const RegimeFeatures& rg, const MicrostructureFeatures& ms) {
    std::vector<Scenario> out;
    if (st.continuation_pressure > 0.5 && rg.current == Regime::TrendUp)
        out.push_back({ScenarioType::Continuation, st.continuation_pressure, st.range_width * 0.5, st.range_width * 0.2});
    if (st.pullback_depth > 0.4)
        out.push_back({ScenarioType::Pullback, st.pullback_depth, st.range_width * 0.3, st.pullback_depth * st.range_width});
    if (rg.current == Regime::Range)
        out.push_back({ScenarioType::Range, rg.confidence, st.range_width * 0.25, st.range_boundary_distance});
    if (st.breakout_strength > 0.5)
        out.push_back({ScenarioType::Breakout, st.breakout_strength, st.range_width, st.range_width * 0.15});
    if (st.failed_breakout > 0.3 || ms.exhaustion_proxy > 0.5)
        out.push_back({ScenarioType::Failure, std::max(st.failed_breakout, ms.exhaustion_proxy), st.range_width * 0.2, 0});
    if (st.reversal_candidate > 0.3 || ms.rejection_proxy > 0.5)
        out.push_back({ScenarioType::Reversal, std::max(st.reversal_candidate, ms.rejection_proxy), st.range_width * 0.4, st.range_width * 0.2});
    return out;
}
}