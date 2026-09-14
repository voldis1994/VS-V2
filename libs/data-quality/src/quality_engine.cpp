#include "mr/data_quality/quality_engine.hpp"
namespace mr {
void QualityEngine::process(const NormalizedEvent& event, double stale_threshold_ms) {
    auto& health = health_map_[event.source];
    health.source = event.source; health.event_count++;
    auto now = event.normalized_timestamp;
    if (last_event_time_.count(event.source)) {
        double gap_ms = static_cast<double>((now - last_event_time_[event.source]).count()) / 1e6;
        health.latency_ms = gap_ms;
        latency_.record(event.source, gap_ms);
        health.status = gap_ms > stale_threshold_ms ? HealthStatus::Degraded : HealthStatus::Healthy;
        if (gap_ms > stale_threshold_ms) stale_count_[event.source]++;
    }
    last_event_time_[event.source] = now;
    health.last_event_time = now;
    freshness_.observe(event.source, now, stale_threshold_ms);
    if (has_flag(event.quality, DataQualityFlag::SequenceGap)) gap_count_[event.source]++;
    if (has_flag(event.quality, DataQualityFlag::Duplicate)) duplicate_count_[event.source]++;
    if (has_flag(event.quality, DataQualityFlag::OutOfOrder)) ooo_count_[event.source]++;
    if (health.event_count > 0) {
        health.stale_rate = static_cast<double>(stale_count_[event.source]) / health.event_count;
        health.sequence_gap_rate = static_cast<double>(gap_count_[event.source]) / health.event_count;
        health.duplicate_rate = static_cast<double>(duplicate_count_[event.source]) / health.event_count;
        health.out_of_order_rate = static_cast<double>(ooo_count_[event.source]) / health.event_count;
        health.reliability = std::max(0.0, 1.0 - health.stale_rate - health.sequence_gap_rate * 0.5);
    }
}
SourceHealth QualityEngine::health(SourceId source) const {
    auto it = health_map_.find(source); return it == health_map_.end() ? SourceHealth{} : it->second;
}
SourceQuality QualityEngine::quality(SourceId source) const {
    SourceQuality q; auto h = health(source);
    q.score = h.reliability; q.is_stale = h.stale_rate > 0.1 || freshness_.is_stale(source);
    if (h.status == HealthStatus::Unhealthy) q.flags |= static_cast<DataQualityFlags>(DataQualityFlag::Stale);
    return q;
}
bool QualityEngine::is_feed_usable(SourceId source) const {
    auto h = health(source);
    return h.status != HealthStatus::Unhealthy && h.reliability > 0.5;
}
void QualityEngine::reset() {
    health_map_.clear(); last_event_time_.clear();
    stale_count_.clear(); gap_count_.clear(); duplicate_count_.clear(); ooo_count_.clear();
}
}