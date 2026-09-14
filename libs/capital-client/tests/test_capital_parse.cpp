#include <gtest/gtest.h>
#include "mr/capital/capital_parse.hpp"
#include <nlohmann/json.hpp>

using namespace mr;

TEST(CapitalParse, NestedAccountEquity) {
    nlohmann::json payload = {
        {"accounts",
         {{{"accountId", "ACC1"},
           {"currency", "USD"},
           {"balance", {{"balance", 12500.5}, {"available", 12000.0}, {"profitLoss", 100.0}}}}}}};
    auto equity = parse_account_equity(payload);
    ASSERT_TRUE(equity.has_value());
    EXPECT_DOUBLE_EQ(*equity, 12500.5);

    auto acc = parse_primary_account(payload);
    ASSERT_TRUE(acc.has_value());
    EXPECT_EQ(acc->account_id, "ACC1");
    EXPECT_DOUBLE_EQ(acc->equity, 12500.5);
}

TEST(CapitalParse, FlatAccountEquity) {
    nlohmann::json payload = {
        {"accounts",
         {{{"accountId", "A2"}, {"balance", 9000.0}, {"available", 8000.0}, {"equity", 9100.0}}}}};
    auto equity = parse_account_equity(payload);
    ASSERT_TRUE(equity.has_value());
    EXPECT_DOUBLE_EQ(*equity, 9100.0);
}

TEST(CapitalParse, MissingEquityReturnsNullopt) {
    nlohmann::json payload = {{"accounts", nlohmann::json::array()}};
    EXPECT_FALSE(parse_account_equity(payload).has_value());
}

TEST(CapitalParse, SnapshotTimeUtcAndNestedPrices) {
    nlohmann::json bar = {{"snapshotTimeUTC", "2024-06-15T12:34:00"},
                          {"openPrice", {{"bid", 2000.0}, {"ask", 2000.1}}},
                          {"highPrice", {{"bid", 2010.0}}},
                          {"lowPrice", {{"bid", 1990.0}}},
                          {"closePrice", {{"bid", 2005.0}}}};
    auto parsed = parse_price_bar(bar, Timestamp(0));
    EXPECT_GT(parsed.time.count(), 0);
    EXPECT_DOUBLE_EQ(parsed.open, 2000.0);
    EXPECT_DOUBLE_EQ(parsed.close, 2005.0);
}

TEST(CapitalParse, FullyClosedBarBoundary) {
    CapitalPriceBar bar;
    bar.time = Timestamp(0);
    bar.close = 100.0;
    EXPECT_TRUE(is_fully_closed_bar(bar, Timeframe::Minute1, Timestamp(60'000'000'000LL)));
    EXPECT_FALSE(is_fully_closed_bar(bar, Timeframe::Minute1, Timestamp(59'000'000'000LL)));
}
