#pragma once
#include "mr/capital/capital_session.hpp"
#include "mr/capital/capital_account.hpp"
#include "mr/capital/capital_quote.hpp"
#include "mr/capital/capital_instrument.hpp"
#include "mr/capital/capital_order.hpp"
#include "mr/capital/capital_prices.hpp"
#include "mr/capital/capital_parse.hpp"
#include "mr/market_types/timeframe.hpp"
#include "mr/common/id.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace mr {

struct CapitalHttpResult {
    int status{0};
    nlohmann::json body;
    [[nodiscard]] bool ok() const { return status >= 200 && status < 300; }
};

class CapitalClient {
public:
    explicit CapitalClient(const std::string& base_url);
    ~CapitalClient();

    bool connect();
    void disconnect();
    [[nodiscard]] bool is_connected() const { return connected_; }
    [[nodiscard]] HealthStatus health() const;

    bool authenticate(const std::string& api_key, const std::string& password, const std::string& identifier);
    /** Re-login using stored credentials after 401 / disconnect. */
    bool reconnect();

    [[nodiscard]] std::optional<CapitalAccount> account_info();
    /** Real account equity for RiskEngine — never invents a default. */
    [[nodiscard]] std::optional<double> account_equity();

    [[nodiscard]] std::optional<CapitalQuote> quote(InstrumentId instrument);
    /**
     * Fetch closed OHLC (Minute1+) — structure/context authority source.
     * Tries /prices/{epic}, /prices?epic=, /history/prices fallbacks (VS-proven).
     */
    [[nodiscard]] CapitalPriceHistory prices(const std::string& epic, Timeframe tf, int max_bars = 100);

    CapitalOrderResponse create_position(const CapitalOrderRequest& request);
    CapitalOrderResponse close_position(const std::string& deal_id);
    [[nodiscard]] std::vector<CapitalPosition> positions();
    CapitalInstrumentMap& instruments() { return instruments_; }
    [[nodiscard]] const CapitalSession& session() const { return session_; }
    [[nodiscard]] int last_http_status() const { return session_.last_http_status; }
    [[nodiscard]] bool rate_limited() const;
    [[nodiscard]] bool needs_reauth() const { return session_.last_http_status == 401; }

private:
    std::string base_url_;
    CapitalSession session_;
    bool connected_{false};
    CapitalInstrumentMap instruments_;
    int max_retries_{3};

    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata);
    void apply_auth_header(const std::string& header);
    void note_status(int status);
    CapitalHttpResult http_request(const std::string& method, const std::string& path,
                                   const nlohmann::json& body = {}, bool with_api_key = true);
    CapitalHttpResult http_request_with_retry(const std::string& method, const std::string& path,
                                              const nlohmann::json& body = {});
};

}  // namespace mr
