#include <gtest/gtest.h>
#include "mr/candle_engine/second_builder.hpp"
using namespace mr;
TEST(SecondBuilder, FormsBar) {
    SecondBuilder b;
    auto ev = b.on_tick(100.0, Timestamp(0), 1);
    EXPECT_EQ(ev.candle.ticks, 1u);
    ev = b.on_tick(101.0, Timestamp(500'000'000LL), 1);
    EXPECT_EQ(ev.candle.close, 101.0);
}
