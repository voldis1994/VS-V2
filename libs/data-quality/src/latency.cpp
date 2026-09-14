#include "mr/data_quality/latency.hpp"
#include "mr/common/statistics.hpp"
#include <vector>
namespace mr {
void LatencyTracker::record(SourceId src, double latency_ms) { samples_[src].push(latency_ms); }
double LatencyTracker::mean_ms(SourceId src) const {
    auto it = samples_.find(src); if (it == samples_.end() || it->second.empty()) return 0;
    std::vector<double> v; for (std::size_t i=0;i<it->second.size();++i) v.push_back(it->second.at(i));
    return mean(v);
}
double LatencyTracker::p95_ms(SourceId src) const {
    auto it = samples_.find(src); if (it == samples_.end() || it->second.empty()) return 0;
    std::vector<double> v; for (std::size_t i=0;i<it->second.size();++i) v.push_back(it->second.at(i));
    return percentile(v, 0.95);
}
}