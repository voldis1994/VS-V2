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
}