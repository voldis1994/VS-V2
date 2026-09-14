#include "mr/market_concepts/concept_weight_config.hpp"
#include <cmath>

namespace mr {
namespace {

void unitize(std::array<double, kConceptFeatureDims>& v) {
    double n2 = 0.0;
    for (double x : v) n2 += x * x;
    const double n = std::sqrt(n2);
    if (n <= 1e-12) return;
    for (double& x : v) x /= n;
}

}  // namespace

ConceptWeightConfig ConceptWeightConfig::defaults() {
    ConceptWeightConfig cfg;
    cfg.feature_scales.fill(1.0);

    // Feature layout (MarketConceptsEngine::normalize_structure):
    //  0 swing_bullish   1 trend_up_mass   2 trend_down_mass  3 trend_strength
    //  4 pullback_depth  5 range_mass      6 range_position   7 compression
    //  8 expansion       9 breakout_up    10 breakout_down   11 breakout_strength
    // 12 failed_bo_up   13 failed_bo_down 14 reversal        15 invalidation
    //
    // Prototypes describe structure shapes — NOT trade entry gates.

    auto set = [&](Regime r, std::array<double, kConceptFeatureDims> p) {
        unitize(p);
        cfg.prototypes[concept_index(r)] = p;
    };

    set(Regime::Unknown,
        {0, 0, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    set(Regime::Range,
        {0.5, 0, 0, 0, 0, 1, 0.5, 0.2, 0, 0, 0, 0, 0, 0, 0, 0});
    set(Regime::TrendUp,
        {1, 1, 0, 1, 0, 0, 0.7, 0, 0.2, 0, 0, 0.1, 0, 0, 0, 0});
    set(Regime::TrendDown,
        {0, 0, 1, 1, 0, 0, 0.3, 0, 0.2, 0, 0, 0.1, 0, 0, 0, 0});
    set(Regime::PullbackUptrend,
        {0.8, 1, 0, 0.7, 1, 0, 0.4, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    set(Regime::PullbackDowntrend,
        {0.2, 0, 1, 0.7, 1, 0, 0.6, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    set(Regime::Compression,
        {0.5, 0.2, 0.2, 0.2, 0, 0.8, 0.5, 1, 0, 0, 0, 0, 0, 0, 0, 0});
    set(Regime::Expansion,
        {0.5, 0.3, 0.3, 0.4, 0, 0.2, 0.5, 0, 1, 0, 0, 0.2, 0, 0, 0, 0});
    set(Regime::BreakoutUp,
        {0.8, 0.7, 0, 0.5, 0, 0, 0.9, 0, 0.6, 1, 0, 1, 0, 0, 0, 0});
    set(Regime::BreakoutDown,
        {0.2, 0, 0.7, 0.5, 0, 0, 0.1, 0, 0.6, 0, 1, 1, 0, 0, 0, 0});
    set(Regime::FailedBreakoutUp,
        {0.4, 0.3, 0.2, 0.3, 0.2, 0.5, 0.5, 0, 0.2, 0.3, 0, 0.3, 1, 0, 0.3, 0.2});
    set(Regime::FailedBreakoutDown,
        {0.6, 0.2, 0.3, 0.3, 0.2, 0.5, 0.5, 0, 0.2, 0, 0.3, 0.3, 0, 1, 0.3, 0.2});
    set(Regime::ReversalCandidate,
        {0.5, 0.3, 0.3, 0.3, 0.3, 0.2, 0.5, 0, 0.3, 0, 0, 0.2, 0.2, 0.2, 1, 0.7});
    set(Regime::Transition,
        {0.5, 0.4, 0.4, 0.3, 0.3, 0.4, 0.5, 0.3, 0.3, 0.2, 0.2, 0.2, 0.2, 0.2, 0.4, 0.4});

    return cfg;
}

}  // namespace mr
