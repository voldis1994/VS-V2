#pragma once
#include "mr/data_quality/quality_state.hpp"
#include "mr/data_quality/freshness.hpp"
#include "mr/data_quality/latency.hpp"
#include "mr/data_quality/anomaly.hpp"
#include "mr/market_types/market_event.hpp"
#include <unordered_map>
namespace mr {
class QualityEngine {
public:
    void process(const NormalizedEvent& event, double stale_threshold_ms);
    [[nodiscard]] SourceHealth health(SourceId source) const;
    [[nodiscard]] SourceQuality quality(SourceId source) const;
    [[nodiscard]] bool is_feed_usable(SourceId source) const;
    void reset();
private:
    std::unordered_map<SourceId, SourceHealth> health_map_;
    std::unordered_map<SourceId, Timestamp> last_event_time_;
    std::unordered_map<SourceId, std::uint64_t> stale_count_, gap_count_, duplicate_count_, ooo_count_;
    FreshnessTracker freshness_;
    LatencyTracker latency_;
    AnomalyDetector anomaly_;
};
}