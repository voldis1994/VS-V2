#include "mr/market_concepts/market_concepts_engine.hpp"
#include <algorithm>
#include <cmath>

namespace mr {

RegimeFeatures MarketConceptsEngine::evaluate(const PriceDynamics& /*pd*/,
                                              const StructureFeatures& st) {
    RegimeFeatures r;
    // Volatility is a structure metric (ATR-like), not acceleration (old dead path).
    r.volatility = st.volatility;
    r.trend_strength = st.trend_strength;
    r.previous = previous_;

    const Regime prev = previous_;
    Regime next = Regime::Unknown;

    // Priority mirrors corrected VS classifier — failed breakouts reachable;
    // pullbacks use structure trend + in_pullback (NOT velocity∧persistence deadlock).
    if (st.failed_breakout_up || (st.failed_breakout > 0.5 && prev == Regime::BreakoutUp)) {
        next = Regime::FailedBreakoutUp;
    } else if (st.failed_breakout_down
               || (st.failed_breakout > 0.5 && prev == Regime::BreakoutDown)) {
        next = Regime::FailedBreakoutDown;
    } else if (st.compression > 0.35 && st.in_range && !st.breakout_active) {
        next = Regime::Compression;
    } else if (st.breakout_up && st.expansion > 0.15) {
        next = Regime::BreakoutUp;
    } else if (st.breakout_down && st.expansion > 0.15) {
        next = Regime::BreakoutDown;
    } else if (st.expansion > 0.45 && !st.breakout_active) {
        next = Regime::Expansion;
    } else if (st.trend_direction == TrendBias::Up && st.in_pullback) {
        next = Regime::PullbackUptrend;
    } else if (st.trend_direction == TrendBias::Down && st.in_pullback) {
        next = Regime::PullbackDowntrend;
    } else if (st.trend_direction == TrendBias::Up && st.trend_strength > 0.25) {
        next = Regime::TrendUp;
    } else if (st.trend_direction == TrendBias::Down && st.trend_strength > 0.25) {
        next = Regime::TrendDown;
    } else if (st.reversal_candidate > 0.5) {
        next = Regime::ReversalCandidate;
    } else if (st.in_range || st.trend_direction == TrendBias::Range) {
        next = Regime::Range;
    } else if (prev != Regime::Unknown && prev != Regime::Range) {
        next = Regime::Transition;
    } else {
        next = Regime::Unknown;
    }

    r.current = next;
    r.confidence = std::clamp(
        0.35 + 0.40 * st.structure_quality + 0.25 * st.trend_strength, 0.0, 0.95);
    if (next == Regime::Unknown) r.confidence = 0;

    previous_ = next;
    return r;
}

}  // namespace mr
