#include <gtest/gtest.h>

#include "mr/market_core/brain_json.hpp"

using namespace mr;

TEST(BrainFeedJson, DefaultRuntimeDoesNotInventOnlineOrZeroRiskMetrics) {
    BrainSnapshot snap;
    snap.id = 1;
    snap.ts = Timestamp{100};
    BrainContext ctx;
    ctx.ts = Timestamp{100};
    snap.instruments.emplace(InstrumentId{1}, ctx);

    const auto j = brain_snapshot_to_json(snap, "m", "1.0.0", "UNKNOWN", BrainFeedRuntime{});

    EXPECT_EQ(j.at("operating_mode"), "UNKNOWN");
    EXPECT_EQ(j.at("health").at("market_core"), "UNKNOWN");
    EXPECT_EQ(j.at("health").at("feeds"), "UNKNOWN");
    EXPECT_EQ(j.at("health").at("execution"), "UNKNOWN");
    EXPECT_EQ(j.at("health").at("data"), "UNKNOWN");
    EXPECT_NE(j.at("health").at("market_core").get<std::string>(), "ONLINE");

    ASSERT_EQ(j.at("instruments").size(), 1u);
    const auto& risk = j.at("instruments").at(0).at("risk");
    EXPECT_TRUE(risk.at("exposure").is_null());
    EXPECT_TRUE(risk.at("daily_pnl").is_null());
    EXPECT_TRUE(risk.at("max_drawdown").is_null());
    EXPECT_TRUE(j.at("instruments").at(0).at("position").at("realized_pnl").is_null());
}

TEST(BrainFeedJson, RealRuntimeValuesPassThrough) {
    BrainSnapshot snap;
    snap.id = 2;
    BrainContext ctx;
    snap.instruments.emplace(InstrumentId{7}, ctx);

    BrainFeedRuntime rt;
    rt.market_core_health = "HEALTHY";
    rt.feeds_health = "DEGRADED";
    rt.execution_health = "UNKNOWN";
    rt.data_health = "HEALTHY";
    rt.exposure = 1250.5;
    rt.daily_pnl = -12.25;
    rt.max_drawdown = 3.5;
    rt.realized_pnl = 40.0;

    const auto j = brain_snapshot_to_json(snap, "m", "1.0.0", "LIVE", rt);
    EXPECT_EQ(j.at("health").at("market_core"), "HEALTHY");
    EXPECT_EQ(j.at("health").at("feeds"), "DEGRADED");
    EXPECT_EQ(j.at("health").at("execution"), "UNKNOWN");
    const auto& risk = j.at("instruments").at(0).at("risk");
    EXPECT_DOUBLE_EQ(risk.at("exposure").get<double>(), 1250.5);
    EXPECT_DOUBLE_EQ(risk.at("daily_pnl").get<double>(), -12.25);
    EXPECT_DOUBLE_EQ(risk.at("max_drawdown").get<double>(), 3.5);
    EXPECT_DOUBLE_EQ(j.at("instruments").at(0).at("position").at("realized_pnl").get<double>(), 40.0);
}
