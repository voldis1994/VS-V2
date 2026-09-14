#!/usr/bin/env python3
"""Generate remaining VS-V2 C++ libraries."""
from pathlib import Path
import textwrap

ROOT = Path("/workspace")

def w(path, content):
    p = ROOT / path
    p.parent.mkdir(parents=True, exist_ok=True)
    c = textwrap.dedent(content).lstrip("\n") if content.startswith("\n") else content
    p.write_text(c)

# ── normalization ─────────────────────────────────────────────────────────
w("libs/normalization/include/mr/normalization/normalizer.hpp", """
#pragma once
#include "mr/market_types/market_event.hpp"
#include "mr/common/clock.hpp"
namespace mr {
class Normalizer {
public:
    explicit Normalizer(Clock& clock) : clock_(clock) {}
    NormalizedEvent normalize(const MarketEvent& event);
private:
    Clock& clock_;
    SequenceNumber last_seq_[256]{};
};
}""")

w("libs/normalization/include/mr/normalization/quote_normalizer.hpp", """
#pragma once
#include "mr/normalization/normalizer.hpp"
namespace mr {
class QuoteNormalizer {
public:
    explicit QuoteNormalizer(Normalizer& n) : base_(n) {}
    NormalizedEvent normalize_quote(const MarketEvent& e);
private:
    Normalizer& base_;
};
}""")

w("libs/normalization/include/mr/normalization/candle_normalizer.hpp", """
#pragma once
#include "mr/market_types/candle.hpp"
namespace mr {
struct NormalizedCandle : Candle { Timestamp normalized_time{}; };
class CandleNormalizer {
public:
    NormalizedCandle normalize(const Candle& c, Timestamp ts);
};
}""")

w("libs/normalization/include/mr/normalization/timestamp_normalizer.hpp", """
#pragma once
#include "mr/market_types/market_event.hpp"
namespace mr {
class TimestampNormalizer {
public:
    Timestamp normalize(const MarketEvent& e, Timestamp receive);
};
}""")

w("libs/normalization/src/normalizer.cpp", """
#include "mr/normalization/normalizer.hpp"
namespace mr {
NormalizedEvent Normalizer::normalize(const MarketEvent& event) {
    NormalizedEvent out = static_cast<NormalizedEvent>(event);
    out.processing_start = clock_.utc_now();
    if (event.exchange_timestamp.count() > 0) {
        out.normalized_timestamp = event.exchange_timestamp;
    } else if (event.provider_timestamp.count() > 0) {
        out.normalized_timestamp = event.provider_timestamp;
    } else {
        out.normalized_timestamp = out.processing_start;
    }
    if (event.source < 256) {
        auto& last = last_seq_[event.source];
        if (event.sequence > 0 && last > 0) {
            if (event.sequence == last) out.quality |= static_cast<DataQualityFlags>(DataQualityFlag::Duplicate);
            else if (event.sequence < last) out.quality |= static_cast<DataQualityFlags>(DataQualityFlag::OutOfOrder);
            else if (event.sequence > last + 1) out.quality |= static_cast<DataQualityFlags>(DataQualityFlag::SequenceGap);
        }
        if (event.sequence > 0) last = event.sequence;
    }
    if (out.bid && out.ask && *out.bid >= *out.ask) {
        out.quality |= static_cast<DataQualityFlags>(DataQualityFlag::Crossed);
    }
    out.processing_end = clock_.utc_now();
    return out;
}
}""")

w("libs/normalization/src/quote_normalizer.cpp", """
#include "mr/normalization/quote_normalizer.hpp"
namespace mr {
NormalizedEvent QuoteNormalizer::normalize_quote(const MarketEvent& e) {
    auto out = base_.normalize(e);
    if (!out.bid || !out.ask) out.quality |= static_cast<DataQualityFlags>(DataQualityFlag::MissingField);
    return out;
}
}""")

w("libs/normalization/src/candle_normalizer.cpp", """
#include "mr/normalization/candle_normalizer.hpp"
namespace mr {
NormalizedCandle CandleNormalizer::normalize(const Candle& c, Timestamp ts) {
    NormalizedCandle n = static_cast<NormalizedCandle>(c);
    n.normalized_time = ts;
    return n;
}
}""")

w("libs/normalization/src/timestamp_normalizer.cpp", """
#include "mr/normalization/timestamp_normalizer.hpp"
namespace mr {
Timestamp TimestampNormalizer::normalize(const MarketEvent& e, Timestamp receive) {
    if (e.exchange_timestamp.count() > 0) return e.exchange_timestamp;
    if (e.provider_timestamp.count() > 0) return e.provider_timestamp;
    return receive;
}
}""")

w("libs/normalization/CMakeLists.txt", """
add_library(mr_normalization STATIC
    src/normalizer.cpp src/quote_normalizer.cpp
    src/candle_normalizer.cpp src/timestamp_normalizer.cpp)
add_library(mr::normalization ALIAS mr_normalization)
target_include_directories(mr_normalization PUBLIC include)
target_link_libraries(mr_normalization PUBLIC mr::common mr::market-types)
target_compile_features(mr_normalization PUBLIC cxx_std_20)
""")

# ── data-quality ────────────────────────────────────────────────────────────
w("libs/data-quality/include/mr/data_quality/quality_state.hpp", """
#pragma once
#include "mr/market_types/data_quality.hpp"
#include "mr/common/id.hpp"
namespace mr {
struct SourceHealth {
    SourceId source{kInvalidSource};
    HealthStatus status{HealthStatus::Disconnected};
    double latency_ms{0}, jitter_ms{0}, stale_rate{0}, disconnect_rate{0};
    double sequence_gap_rate{0}, duplicate_rate{0}, out_of_order_rate{0};
    double divergence{0}, reliability{1.0}, predictive_usefulness{0};
    Timestamp last_event_time{};
    std::uint64_t event_count{0};
};
struct SourceQuality { double score{0}; DataQualityFlags flags{0}; bool is_stale{false}; };
struct SourceWeight { SourceId source{kInvalidSource}; double weight{0}; };
}""")

w("libs/data-quality/include/mr/data_quality/freshness.hpp", """
#pragma once
#include "mr/data_quality/quality_state.hpp"
namespace mr {
class FreshnessTracker {
public:
    void observe(SourceId src, Timestamp ts, double stale_ms);
    [[nodiscard]] bool is_stale(SourceId src) const;
    [[nodiscard]] double age_ms(SourceId src, Timestamp now) const;
private:
    std::unordered_map<SourceId, Timestamp> last_;
    std::unordered_map<SourceId, double> threshold_;
};
}""")

w("libs/data-quality/include/mr/data_quality/latency.hpp", """
#pragma once
#include "mr/common/ring_buffer.hpp"
#include "mr/common/id.hpp"
namespace mr {
class LatencyTracker {
public:
    void record(SourceId src, double latency_ms);
    [[nodiscard]] double mean_ms(SourceId src) const;
    [[nodiscard]] double p95_ms(SourceId src) const;
private:
    std::unordered_map<SourceId, RingBuffer<double, 256>> samples_;
};
}""")

w("libs/data-quality/include/mr/data_quality/anomaly.hpp", """
#pragma once
#include "mr/market_types/market_event.hpp"
namespace mr {
struct AnomalyResult { bool crossed{false}; bool wide_spread{false}; bool price_jump{false}; };
class AnomalyDetector {
public:
    void configure(double max_spread, double max_jump_pct);
    AnomalyResult check(const NormalizedEvent& e, double prev_mid);
private:
    double max_spread_{10.0}, max_jump_pct_{0.05};
};
}""")

w("libs/data-quality/include/mr/data_quality/quality_engine.hpp", """
#pragma once
#include "mr/data_quality/quality_state.hpp"
#include "mr/data_quality/freshness.hpp"
#include "mr/data_quality/latency.hpp"
#include "mr/data_quality/anomaly.hpp"
#include "mr/market_types/market_event.hpp"
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
}""")

w("libs/data-quality/src/freshness.cpp", """
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
}""")

w("libs/data-quality/src/latency.cpp", """
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
}""")

w("libs/data-quality/src/anomaly.cpp", """
#include "mr/data_quality/anomaly.hpp"
#include "mr/common/math.hpp"
namespace mr {
void AnomalyDetector::configure(double max_spread, double max_jump_pct) {
    max_spread_ = max_spread; max_jump_pct_ = max_jump_pct;
}
AnomalyResult AnomalyDetector::check(const NormalizedEvent& e, double prev_mid) {
    AnomalyResult r;
    if (e.bid && e.ask) {
        r.crossed = *e.bid >= *e.ask;
        r.wide_spread = (*e.ask - *e.bid) > max_spread_;
        double mid = (*e.bid + *e.ask) * 0.5;
        if (prev_mid > 0) r.price_jump = std::abs(pct_change(prev_mid, mid)) > max_jump_pct_;
    }
    return r;
}
}""")

w("libs/data-quality/src/quality_engine.cpp", """
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
}""")

w("libs/data-quality/CMakeLists.txt", """
add_library(mr_data_quality STATIC
    src/quality_engine.cpp src/freshness.cpp src/latency.cpp src/anomaly.cpp)
add_library(mr::data-quality ALIAS mr_data_quality)
target_include_directories(mr_data_quality PUBLIC include)
target_link_libraries(mr_data_quality PUBLIC mr::common mr::market-types)
target_compile_features(mr_data_quality PUBLIC cxx_std_20)
""")

print("normalization + data-quality done")
