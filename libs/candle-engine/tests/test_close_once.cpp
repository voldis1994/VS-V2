#include <gtest/gtest.h>
#include "mr/candle_engine/candle_engine.hpp"
using namespace mr;
TEST(CandleEngine, CloseOncePerBucket) {
    CandleEngine eng;
    for (int i = 0; i < 5; ++i)
        eng.on_quote(100.0 + i * 0.1, Timestamp(static_cast<long long>(i) * 1'000'000'000LL), 1);
    EXPECT_FALSE(eng.state().has_closed);
    eng.on_quote(101.0, Timestamp(10'000'000'000LL), 1);
    EXPECT_TRUE(eng.state().has_closed);
    EXPECT_TRUE(eng.state().last_closed_10s.status == CandleStatus::Closed);
    auto closed_open = eng.state().last_closed_10s.open_time;
    eng.on_quote(102.0, Timestamp(11'000'000'000LL), 1);
    EXPECT_EQ(eng.state().last_closed_10s.open_time, closed_open);
}
