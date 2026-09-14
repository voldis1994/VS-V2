#pragma once
#include "mr/common/ring_buffer.hpp"
#include <unordered_map>
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
}