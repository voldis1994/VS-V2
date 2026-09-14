#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

namespace mr {

inline double clamp(double v, double lo, double hi) {
    return std::max(lo, std::min(v, hi));
}

inline double safe_div(double a, double b, double fallback = 0.0) {
    return std::abs(b) > std::numeric_limits<double>::epsilon() ? a / b : fallback;
}

inline double sigmoid(double x) { return 1.0 / (1.0 + std::exp(-x)); }

inline double pct_change(double from, double to) {
    return safe_div(to - from, from);
}

}  // namespace mr
