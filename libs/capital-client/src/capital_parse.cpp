#include "mr/capital/capital_parse.hpp"
#include <cmath>
#include <cstdlib>
#include <ctime>

namespace mr {
namespace {

double num_or_nan(const nlohmann::json& v) {
    if (v.is_number()) return v.get<double>();
    if (v.is_string()) {
        try {
            return std::stod(v.get<std::string>());
        } catch (...) {
            return std::nan("");
        }
    }
    return std::nan("");
}

double nested_price(const nlohmann::json& p, const char* key) {
    if (!p.contains(key)) return std::nan("");
    const auto& node = p[key];
    if (node.is_number() || node.is_string()) return num_or_nan(node);
    if (node.is_object()) {
        if (node.contains("bid")) {
            const double bid = num_or_nan(node["bid"]);
            if (std::isfinite(bid)) return bid;
        }
        if (node.contains("ask")) {
            const double ask = num_or_nan(node["ask"]);
            if (std::isfinite(ask)) return ask;
        }
    }
    return std::nan("");
}

Timestamp parse_iso8601_utc(const std::string& s) {
    // Accept "YYYY-MM-DDTHH:MM:SS" or with fractional / Z / space separator.
    if (s.size() < 19) return {};
    std::tm tm{};
    tm.tm_year = std::atoi(s.substr(0, 4).c_str()) - 1900;
    tm.tm_mon = std::atoi(s.substr(5, 2).c_str()) - 1;
    tm.tm_mday = std::atoi(s.substr(8, 2).c_str());
    tm.tm_hour = std::atoi(s.substr(11, 2).c_str());
    tm.tm_min = std::atoi(s.substr(14, 2).c_str());
    tm.tm_sec = std::atoi(s.substr(17, 2).c_str());
#if defined(_WIN32)
    const time_t sec = _mkgmtime(&tm);
#else
    const time_t sec = timegm(&tm);
#endif
    if (sec < 0) return {};
    return Timestamp(static_cast<long long>(sec) * 1'000'000'000LL);
}

}  // namespace

Timestamp parse_capital_timestamp(const nlohmann::json& bar, Timestamp fallback) {
    if (bar.contains("snapshotTimeUTC") && bar["snapshotTimeUTC"].is_string()) {
        auto ts = parse_iso8601_utc(bar["snapshotTimeUTC"].get<std::string>());
        if (ts.count() > 0) return ts;
    }
    if (bar.contains("snapshotTime") && bar["snapshotTime"].is_string()) {
        auto ts = parse_iso8601_utc(bar["snapshotTime"].get<std::string>());
        if (ts.count() > 0) return ts;
    }
    if (bar.contains("time")) {
        if (bar["time"].is_number_integer()) {
            const auto v = bar["time"].get<long long>();
            // Heuristic: seconds vs ms vs ns
            if (v > 1'000'000'000'000'000LL) return Timestamp(v);           // ns
            if (v > 1'000'000'000'000LL) return Timestamp(v * 1'000'000LL); // ms
            return Timestamp(v * 1'000'000'000LL);                          // s
        }
        if (bar["time"].is_string()) {
            auto ts = parse_iso8601_utc(bar["time"].get<std::string>());
            if (ts.count() > 0) return ts;
        }
    }
    return fallback.count() > 0 ? fallback : now_utc_ns();
}


std::optional<double> parse_account_equity(const nlohmann::json& accounts_payload) {
    auto acc = parse_primary_account(accounts_payload);
    if (!acc) return std::nullopt;
    if (acc->equity > 0.0) return acc->equity;
    if (acc->balance > 0.0) return acc->balance;
    return std::nullopt;
}

std::optional<CapitalAccount> parse_primary_account(const nlohmann::json& accounts_payload) {
    if (accounts_payload.empty()) return std::nullopt;
    const nlohmann::json* accounts = nullptr;
    if (accounts_payload.contains("accounts") && accounts_payload["accounts"].is_array()) {
        accounts = &accounts_payload["accounts"];
    } else if (accounts_payload.is_array()) {
        accounts = &accounts_payload;
    }
    if (!accounts || accounts->empty()) return std::nullopt;

    const auto& acc = (*accounts)[0];
    CapitalAccount info;
    info.account_id = acc.value("accountId", acc.value("account_id", ""));
    info.currency = acc.value("currency", "USD");

    if (acc.contains("balance") && acc["balance"].is_object()) {
        const auto& b = acc["balance"];
        info.balance = num_or_nan(b.contains("balance") ? b["balance"] : nlohmann::json{});
        info.available = num_or_nan(b.contains("available") ? b["available"] : nlohmann::json{});
        info.equity = num_or_nan(b.contains("balance") ? b["balance"] : nlohmann::json{});
        if (!std::isfinite(info.equity) || info.equity <= 0) {
            // Some payloads expose deposit+PnL as equity proxy.
            const double deposit = num_or_nan(b.contains("deposit") ? b["deposit"] : nlohmann::json{});
            const double pl = num_or_nan(b.contains("profitLoss") ? b["profitLoss"] : nlohmann::json{});
            if (std::isfinite(deposit)) {
                info.equity = deposit + (std::isfinite(pl) ? pl : 0.0);
            }
        }
    } else {
        info.balance = num_or_nan(acc.contains("balance") ? acc["balance"] : nlohmann::json{});
        info.available = num_or_nan(acc.contains("available") ? acc["available"] : nlohmann::json{});
        info.equity = num_or_nan(acc.contains("equity") ? acc["equity"] : nlohmann::json{});
        if (!std::isfinite(info.equity) || info.equity <= 0) info.equity = info.balance;
    }

    if (!std::isfinite(info.balance)) info.balance = 0;
    if (!std::isfinite(info.available)) info.available = 0;
    if (!std::isfinite(info.equity)) info.equity = 0;
    if (info.account_id.empty() && info.equity <= 0 && info.balance <= 0) return std::nullopt;
    return info;
}

CapitalPriceBar parse_price_bar(const nlohmann::json& p, Timestamp fallback_ts) {
    CapitalPriceBar bar;
    double open = nested_price(p, "openPrice");
    double high = nested_price(p, "highPrice");
    double low = nested_price(p, "lowPrice");
    double close = nested_price(p, "closePrice");
    if (!std::isfinite(open)) open = num_or_nan(p.contains("open") ? p["open"] : (p.contains("o") ? p["o"] : nlohmann::json{}));
    if (!std::isfinite(high)) high = num_or_nan(p.contains("high") ? p["high"] : (p.contains("h") ? p["h"] : nlohmann::json{}));
    if (!std::isfinite(low)) low = num_or_nan(p.contains("low") ? p["low"] : (p.contains("l") ? p["l"] : nlohmann::json{}));
    if (!std::isfinite(close)) close = num_or_nan(p.contains("close") ? p["close"] : (p.contains("c") ? p["c"] : nlohmann::json{}));
    bar.open = std::isfinite(open) ? open : 0;
    bar.high = std::isfinite(high) ? high : 0;
    bar.low = std::isfinite(low) ? low : 0;
    bar.close = std::isfinite(close) ? close : 0;
    bar.time = parse_capital_timestamp(p, fallback_ts);
    return bar;
}

bool is_fully_closed_bar(const CapitalPriceBar& bar, Timeframe tf, Timestamp now) {
    // open_time == 0 (epoch) is valid; only reject negative / empty OHLC.
    if (bar.time.count() < 0 || bar.close <= 0) return false;
    const auto close_at = bar.time.count() + static_cast<long long>(timeframe_ns(tf));
    return close_at <= now.count();
}

}  // namespace mr
