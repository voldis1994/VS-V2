#include <gtest/gtest.h>
#include "mr/candle_engine/candle_engine.hpp"
#include "mr/market_types/market_event.hpp"

using namespace mr;

static NormalizedEvent make_quote(double mid, Timestamp ts) {
    NormalizedEvent e;
    e.type = MarketEventType::Quote;
    e.normalized_timestamp = ts;
    e.bid = mid - 0.05;
    e.ask = mid + 0.05;
    e.last = mid;
    return e;
}

TEST(TenSecondOhlc, BuildsAndClosesBars) {
    CandleEngine eng;
    for (int i = 0; i < 5; ++i) {
        auto ts = Timestamp(static_cast<long long>(i) * 1'000'000'000LL);
        eng.on_event(make_quote(100.0 + i * 0.1, ts), 100.0 + i * 0.1);
    }
    auto st = eng.state();
    EXPECT_TRUE(st.has_forming);
    EXPECT_FALSE(st.has_closed);
    EXPECT_DOUBLE_EQ(st.forming_10s.open, 100.0);
    EXPECT_GT(st.forming_10s.close, 100.0);

    eng.on_event(make_quote(101.0, Timestamp(10'000'000'000LL)), 101.0);
    st = eng.state();
    EXPECT_TRUE(st.has_closed);
    EXPECT_EQ(st.last_closed_10s.status, CandleStatus::Closed);
    EXPECT_DOUBLE_EQ(st.last_closed_10s.open, 100.0);
    EXPECT_GT(st.last_closed_10s.close, 100.0);
    EXPECT_DOUBLE_EQ(st.forming_10s.open, 101.0);
}

TEST(TenSecondOhlc, BodyPctPositiveOnUpBar) {
    CandleEngine eng;
    eng.on_event(make_quote(2000.0, Timestamp(0)), 2000.0);
    eng.on_event(make_quote(2002.0, Timestamp(2'000'000'000LL)), 2002.0);
    eng.on_event(make_quote(2001.0, Timestamp(10'000'000'000LL)), 2001.0);
    auto st = eng.state();
    ASSERT_TRUE(st.has_closed);
    EXPECT_GT(st.last_closed_10s.body_pct(), 0.0);
}
