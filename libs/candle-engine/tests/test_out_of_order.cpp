#include <gtest/gtest.h>
#include "mr/candle_engine/candle_deduplicator.hpp"
using namespace mr;
TEST(Deduplicator, RejectsDuplicate) {
    CandleDeduplicator d;
    Candle c; c.open_time = Timestamp(0);
    EXPECT_TRUE(d.accept(c));
    EXPECT_FALSE(d.accept(c));
}
