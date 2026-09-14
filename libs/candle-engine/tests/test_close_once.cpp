#include <gtest/gtest.h>
#include "mr/candle_engine/candle_engine.hpp"
using namespace mr;
TEST(CandleEngine, CloseOncePerBucket) {
    CandleEngine eng;
    std::size_t closes = 0;
    for (int i = 0; i < 5; ++i) {
        auto evs = eng.on_quote(100.0 + i * 0.1, Timestamp(static_cast<long long>(i) * 1'000'000'000LL), 1);
        for (const auto& e : evs) if (e.kind == MarketClockKind::ClosedTenSecond) ++closes;
    }
    EXPECT_FALSE(eng.state().has_closed);
    EXPECT_EQ(closes, 0u);

    auto evs = eng.on_quote(101.0, Timestamp(10'000'000'000LL), 1);
    std::size_t shot = 0;
    for (const auto& e : evs) {
        if (e.kind == MarketClockKind::ClosedTenSecond) {
            EXPECT_TRUE(e.one_shot);
            EXPECT_FALSE(e.structure_authority);
            ++shot;
        }
    }
    EXPECT_EQ(shot, 1u);
    EXPECT_TRUE(eng.state().has_closed);
    EXPECT_TRUE(eng.state().last_closed_10s.status == CandleStatus::Closed);
    auto closed_open = eng.state().last_closed_10s.open_time;

    // Further quotes in next bucket must not re-emit the same 10s close.
    evs = eng.on_quote(102.0, Timestamp(11'000'000'000LL), 1);
    for (const auto& e : evs) {
        if (e.kind == MarketClockKind::ClosedTenSecond) {
            EXPECT_NE(e.candle->open_time, closed_open);
        }
    }
    EXPECT_EQ(eng.state().last_closed_10s.open_time, closed_open);
}
