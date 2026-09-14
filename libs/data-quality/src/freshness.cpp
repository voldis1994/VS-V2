#include "mr/data_quality/freshness.hpp"
namespace mr {
void FreshnessTracker::observe(SourceId src, Timestamp ts, double stale_ms) {
    last_[src] = ts; threshold_[src] = stale_ms;
}
bool FreshnessTracker::is_stale(SourceId src) const {
    auto it = last_.find(src); return it == last_.end();
}
double FreshnessTracker::age_ms(SourceId src, Timestamp now) const {
    auto it = last_.find(src);
    if (it == last_.end()) return 1e9;
    return static_cast<double>((now - it->second).count()) / 1e6;
}
}