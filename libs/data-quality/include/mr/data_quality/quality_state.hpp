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
}