#pragma once
#include "mr/capital/capital_session.hpp"
#include "mr/capital/capital_account.hpp"
#include "mr/capital/capital_quote.hpp"
#include "mr/capital/capital_instrument.hpp"
#include "mr/capital/capital_order.hpp"
#include "mr/capital/capital_prices.hpp"
#include "mr/market_types/timeframe.hpp"
#include "mr/common/id.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace mr {

class CapitalClient {
public:
    explicit CapitalClient(const std::string& base_url);
    ~CapitalClient();
    bool connect();
    void disconnect();
    [[nodiscard]] bool is_connected() const { return connected_; }
    [[nodiscard]] HealthStatus health() const;
    bool authenticate(const std::string& api_key, const std::string& password, const std::string& identifier);
    [[nodiscard]] std::optional<CapitalAccount> account_info();
    [[nodiscard]] std::optional<CapitalQuote> quote(InstrumentId instrument);
    /** Fetch closed OHLC (Minute1+) — structure/context authority source. */
    [[nodiscard]] CapitalPriceHistory prices(const std::string& epic, Timeframe tf, int max_bars = 100);
    CapitalOrderResponse create_position(const CapitalOrderRequest& request);
    CapitalOrderResponse close_position(const std::string& deal_id);
    [[nodiscard]] std::vector<CapitalPosition> positions();
    CapitalInstrumentMap& instruments() { return instruments_; }

private:
    std::string base_url_;
    CapitalSession session_;
    bool connected_{false};
    CapitalInstrumentMap instruments_;
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata);
    void apply_auth_header(const std::string& header);
    nlohmann::json http_request(const std::string& method, const std::string& path, const nlohmann::json& body = {});
};

}  // namespace mr
