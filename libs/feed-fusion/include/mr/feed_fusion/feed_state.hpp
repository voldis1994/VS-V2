#pragma once
#include "mr/market_types/market_event.hpp"
#include "mr/data_quality/quality_state.hpp"
#include "mr/common/ring_buffer.hpp"
#include <unordered_map>
namespace mr {
struct ReactionEvent { SourceId source{kInvalidSource}; Timestamp timestamp{}; double price_change{0}; };
struct InstrumentFeedState {
    std::unordered_map<SourceId, NormalizedEvent> last_by_source;
    std::unordered_map<SourceId, SourceHealth> health_by_source;
    RingBuffer<ReactionEvent, 64> reactions;
    double last_mid{0};
};
}