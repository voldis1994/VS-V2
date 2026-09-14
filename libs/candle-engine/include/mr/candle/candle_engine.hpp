#pragma once

#include "mr/common/market_event.hpp"
#include "mr/feed_fusion/feed_fusion_engine.hpp"
#include <deque>
#include <unordered_map>
#include <vector>

namespace mr {

struct CandleBar {
    InstrumentId instrument{kInvalidInstrument};
    Timestamp bucket_start{};
    double open{0};
    double high{0};
    double low{0};
    double close{0};
    double volume{0};
    std::uint32_t tick_count{0};
};

class CandleEngine {
public:
    explicit CandleEngine(std::int64_t bucket_ns = 10'000'000'000LL);
    void ingest(InstrumentId instrument, const FeedConsensus& consensus, Timestamp ts);
    [[nodiscard]] std::vector<CandleBar> recent(InstrumentId instrument, std::size_t limit = 64) const;
    [[nodiscard]] CandleBar latest(InstrumentId instrument) const;

private:
    std::int64_t bucket_ns_;
    struct State {
        CandleBar current{};
        std::deque<CandleBar> history;
    };
    std::unordered_map<InstrumentId, State> states_;
    void roll_bucket(State& state, InstrumentId instrument, Timestamp ts);
};

}  // namespace mr
