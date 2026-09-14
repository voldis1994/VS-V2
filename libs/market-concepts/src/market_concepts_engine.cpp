#include "mr/market_concepts/market_concepts_engine.hpp"
#include <algorithm>
#include <cmath>

namespace mr {
namespace {

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

double soft_flag(bool flag, double intensity) {
    return flag ? clamp01(intensity) : 0.0;
}

double cosine_similarity(const std::array<double, kConceptFeatureDims>& a,
                         const std::array<double, kConceptFeatureDims>& b) {
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (std::size_t i = 0; i < kConceptFeatureDims; ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    const double denom = std::sqrt(na) * std::sqrt(nb);
    if (denom <= 1e-12) return 0.0;
    // Map cosine [-1,1] → [0,1] for descriptive scores.
    return clamp01(0.5 * (dot / denom) + 0.5);
}

}  // namespace

std::array<double, kConceptFeatureDims>
MarketConceptsEngine::normalize_structure(const StructureFeatures& st) {
    std::array<double, kConceptFeatureDims> f{};

    // Soft directional masses from authoritative structure (not concept triggers).
    const double up_mass =
        (st.trend_direction == TrendBias::Up) ? clamp01(st.trend_strength) : 0.0;
    const double down_mass =
        (st.trend_direction == TrendBias::Down) ? clamp01(st.trend_strength) : 0.0;
    const double range_mass =
        (st.trend_direction == TrendBias::Range || st.in_range)
            ? clamp01(1.0 - st.trend_strength)
            : (st.in_range ? 0.5 : 0.0);

    f[0] = clamp01(0.5 * (st.swing_state + 1.0));
    f[1] = up_mass;
    f[2] = down_mass;
    f[3] = clamp01(st.trend_strength);
    f[4] = clamp01(st.pullback_depth);
    f[5] = range_mass;
    f[6] = clamp01(st.range_position);
    f[7] = clamp01(st.compression);
    f[8] = clamp01(st.expansion);
    f[9] = soft_flag(st.breakout_up, std::max(st.breakout_strength, st.expansion));
    f[10] = soft_flag(st.breakout_down, std::max(st.breakout_strength, st.expansion));
    f[11] = clamp01(st.breakout_strength);
    f[12] = soft_flag(st.failed_breakout_up, st.failed_breakout);
    f[13] = soft_flag(st.failed_breakout_down, st.failed_breakout);
    f[14] = clamp01(st.reversal_candidate);
    f[15] = clamp01(st.structural_invalidation);
    return f;
}

RegimeFeatures MarketConceptsEngine::evaluate(const PriceDynamics& /*pd*/,
                                              const StructureFeatures& st) {
    RegimeFeatures out;
    out.volatility = st.volatility;
    out.trend_strength = st.trend_strength;
    out.previous_dominant = previous_dominant_;
    out.previous = previous_dominant_;

    auto features = normalize_structure(st);
    for (std::size_t i = 0; i < kConceptFeatureDims; ++i) {
        features[i] *= weights_.feature_scales[i];
    }

    // Score ALL 14 concepts simultaneously — no exclusive state machine.
    for (std::size_t c = 0; c < kConceptCount; ++c) {
        out.scores[c] = cosine_similarity(features, weights_.prototypes[c]);
    }

    // Dominant = descriptive argmax only (compat); never a trade trigger.
    std::size_t best = 0;
    for (std::size_t c = 1; c < kConceptCount; ++c) {
        if (out.scores[c] > out.scores[best]) best = c;
    }
    out.dominant = static_cast<Regime>(best);
    out.current = out.dominant;
    out.confidence = out.scores[best];

    previous_dominant_ = out.dominant;
    return out;
}

}  // namespace mr
