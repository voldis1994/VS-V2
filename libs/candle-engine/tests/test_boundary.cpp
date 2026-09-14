#include <gtest/gtest.h>
#include "mr/candle_engine/candle_engine.hpp"
using namespace mr;
TEST(CandleEngine, BoundaryProgress) {
    CandleEngine eng;
    eng.on_quote(100.0, Timestamp(5'000'000'000LL), 1);
    auto st = eng.state();
    EXPECT_NEAR(st.bucket_progress, 0.5, 0.01);
}
