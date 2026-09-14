#include <gtest/gtest.h>
#include "mr/candle_engine/candle_gap_handler.hpp"
#include "mr/market_types/timeframe.hpp"
using namespace mr;
TEST(GapHandler, DetectsMissingBuckets) {
    CandleGapHandler h;
    Candle a, b; a.open_time = Timestamp(0); b.open_time = Timestamp(30'000'000'000LL);
    auto gaps = h.detect_gaps(a, b, timeframe_ns(Timeframe::Second10));
    EXPECT_EQ(gaps.size(), 2u);
}
