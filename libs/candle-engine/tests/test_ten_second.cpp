#include <gtest/gtest.h>
#include "mr/candle_engine/ten_second_builder.hpp"
using namespace mr;
TEST(TenSecondBuilder, ClosesOnBoundary) {
    TenSecondBuilder b;
    for (int i = 0; i < 5; ++i) b.on_tick(100.0 + i, Timestamp(static_cast<long long>(i) * 1'000'000'000LL), 1);
    auto ev = b.on_tick(105.0, Timestamp(10'000'000'000LL), 1);
    EXPECT_EQ(ev.type, CandleEventType::Closed);
    EXPECT_EQ(ev.type, CandleEventType::Closed);
    EXPECT_DOUBLE_EQ(ev.candle.open, 100.0);
    EXPECT_DOUBLE_EQ(ev.candle.close, 104.0);
}
