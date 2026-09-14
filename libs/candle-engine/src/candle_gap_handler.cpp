#include "mr/candle_engine/candle_gap_handler.hpp"
namespace mr {
std::vector<Candle> CandleGapHandler::detect_gaps(const Candle& prev, const Candle& next, std::uint64_t bucket_ns) {
    std::vector<Candle> gaps;
    auto expected = static_cast<std::uint64_t>(prev.open_time.count()) + bucket_ns;
    auto actual = static_cast<std::uint64_t>(next.open_time.count());
    while (expected < actual) {
        Candle g; g.open_time = Timestamp(static_cast<long long>(expected));
        g.status = CandleStatus::Gap; gaps.push_back(g);
        expected += bucket_ns;
    }
    return gaps;
}
}