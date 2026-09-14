#pragma once
#include "mr/capital/capital_account.hpp"
#include "mr/capital/capital_prices.hpp"
#include "mr/common/clock.hpp"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace mr {

/** Parse Capital snapshotTimeUTC / snapshotTime into epoch nanoseconds. */
[[nodiscard]] Timestamp parse_capital_timestamp(const nlohmann::json& bar, Timestamp fallback = {});

/** Nested or flat account equity (balance.balance | balance | equity). */
[[nodiscard]] std::optional<double> parse_account_equity(const nlohmann::json& accounts_payload);

[[nodiscard]] std::optional<CapitalAccount> parse_primary_account(const nlohmann::json& accounts_payload);

[[nodiscard]] CapitalPriceBar parse_price_bar(const nlohmann::json& p, Timestamp fallback_ts = {});

/** True when bar open + timeframe is fully closed relative to `now`. */
[[nodiscard]] bool is_fully_closed_bar(const CapitalPriceBar& bar, Timeframe tf, Timestamp now);

}  // namespace mr
