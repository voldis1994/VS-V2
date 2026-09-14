#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace mr {

/** Simultaneous market concepts (descriptive context — never trade triggers). */
inline constexpr std::size_t kConceptCount = 14;

/**
 * 14 market concepts. Multiple may be active via continuous similarity scores.
 * These describe structure context — they are NOT BUY/SELL triggers.
 */
enum class Regime : std::uint8_t {
    Unknown = 0,
    Range = 1,
    TrendUp = 2,
    TrendDown = 3,
    PullbackUptrend = 4,
    PullbackDowntrend = 5,
    Compression = 6,
    Expansion = 7,
    BreakoutUp = 8,
    BreakoutDown = 9,
    FailedBreakoutUp = 10,
    FailedBreakoutDown = 11,
    ReversalCandidate = 12,
    Transition = 13
};

inline constexpr std::size_t concept_index(Regime r) noexcept {
    return static_cast<std::size_t>(r);
}

/**
 * Multi-concept market description.
 * `scores` — simultaneous similarity of each concept to normalized structure.
 * `current` / `dominant` — argmax for backward-compatible readers (not exclusive lock).
 * Raw StructureFeatures remain the primary truth.
 */
struct RegimeFeatures {
    std::array<double, kConceptCount> scores{};
    Regime dominant{Regime::Unknown};
    Regime previous_dominant{Regime::Unknown};
    double volatility{0};
    double trend_strength{0};
    /** Score of dominant concept (descriptive magnitude, not trade confidence). */
    double confidence{0};

    /** Legacy aliases for existing consumers. */
    Regime current{Regime::Unknown};
    Regime previous{Regime::Unknown};

    [[nodiscard]] double score(Regime r) const noexcept {
        return scores[concept_index(r)];
    }
};

inline const char* regime_name(Regime r) {
    switch (r) {
        case Regime::Unknown: return "UNKNOWN";
        case Regime::Range: return "RANGE";
        case Regime::TrendUp: return "TREND_UP";
        case Regime::TrendDown: return "TREND_DOWN";
        case Regime::PullbackUptrend: return "PULLBACK_UPTREND";
        case Regime::PullbackDowntrend: return "PULLBACK_DOWNTREND";
        case Regime::Compression: return "COMPRESSION";
        case Regime::Expansion: return "EXPANSION";
        case Regime::BreakoutUp: return "BREAKOUT_UP";
        case Regime::BreakoutDown: return "BREAKOUT_DOWN";
        case Regime::FailedBreakoutUp: return "FAILED_BREAKOUT_UP";
        case Regime::FailedBreakoutDown: return "FAILED_BREAKOUT_DOWN";
        case Regime::ReversalCandidate: return "REVERSAL_CANDIDATE";
        case Regime::Transition: return "TRANSITION";
    }
    return "UNKNOWN";
}

}  // namespace mr
