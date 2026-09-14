#include <gtest/gtest.h>
#include "mr/common/clock.hpp"

TEST(Clock, UtcAndSimulated) {
    mr::SystemClock sys;
    auto t0 = sys.utc_now();
    EXPECT_GE(t0.count(), 0);

    mr::SimulatedClock sim;
    sim.set(mr::Timestamp(1'000'000'000LL));
    EXPECT_EQ(sim.utc_now().count(), 1'000'000'000LL);
    sim.advance(500'000'000LL);
    EXPECT_EQ(sim.utc_now().count(), 1'500'000'000LL);
}
