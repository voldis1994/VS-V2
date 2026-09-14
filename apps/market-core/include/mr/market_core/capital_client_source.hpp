#pragma once
#include "mr/market_core/capital_market_source.hpp"
#include "mr/capital/capital_client.hpp"

namespace mr {

/** Adapts CapitalClient to ICapitalMarketSource for the live path. */
class CapitalClientMarketSource final : public ICapitalMarketSource {
public:
    explicit CapitalClientMarketSource(CapitalClient& client) : client_(client) {}

    bool ensure_session() override {
        if (client_.is_connected() && client_.session().active) return true;
        return client_.reconnect();
    }

    void invalidate_session() override { client_.disconnect(); }

    std::optional<CapitalQuote> fetch_quote(InstrumentId instrument) override {
        return client_.quote(instrument);
    }

    CapitalPriceHistory fetch_closed_ohlc(const std::string& epic, Timeframe tf, int max_bars) override {
        return client_.prices(epic, tf, max_bars);
    }

    std::optional<double> fetch_equity() override { return client_.account_equity(); }

    std::uint64_t reconnect_count() const override { return client_.session().reconnect_count; }

private:
    CapitalClient& client_;
};

}  // namespace mr
