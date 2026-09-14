#pragma once
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace mr {

inline double mean(const std::vector<double>& v) {
    if (v.empty()) return 0;
    return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
}

inline double stddev(const std::vector<double>& v) {
    if (v.size() < 2) return 0;
    double m = mean(v);
    double sq = 0;
    for (double x : v) sq += (x - m) * (x - m);
    return std::sqrt(sq / static_cast<double>(v.size()));
}

inline double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    auto idx = static_cast<std::size_t>(p * (v.size() - 1));
    return v[idx];
}

inline double zscore(double x, double m, double s) {
    return s > 1e-12 ? (x - m) / s : 0;
}

}  // namespace mr
