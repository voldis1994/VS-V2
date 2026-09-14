#pragma once
#include <cstdint>
namespace mr {
enum class Regime : std::uint8_t { Unknown=0, Range=1, TrendUp=2, TrendDown=3, Volatile=4 };
struct RegimeFeatures { Regime current{Regime::Unknown}; double confidence{0}; double volatility{0}; double trend_strength{0}; };
inline const char* regime_name(Regime r) {
    switch(r) {
        case Regime::Range: return "RANGE"; case Regime::TrendUp: return "TREND_UP";
        case Regime::TrendDown: return "TREND_DOWN"; case Regime::Volatile: return "VOLATILE"; default: return "UNKNOWN";
    }
}
}