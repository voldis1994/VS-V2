#include "mr/capital/capital_client.hpp"
#include "mr/common/clock.hpp"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

namespace mr {

namespace {
constexpr long long kCooldown429Ns = 120LL * 1'000'000'000LL;  // 120s — VS pool pattern
}

CapitalClient::CapitalClient(const std::string& base_url) : base_url_(base_url) {}
CapitalClient::~CapitalClient() = default;

size_t CapitalClient::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    const auto total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

size_t CapitalClient::header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    static_cast<CapitalClient*>(userdata)->apply_auth_header(std::string(buffer, size * nitems));
    return size * nitems;
}

void CapitalClient::apply_auth_header(const std::string& header) {
    auto trim = [](std::string& s) {
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ')) s.pop_back();
        const auto pos = s.find_first_not_of(' ');
        if (pos != std::string::npos && pos > 0) s.erase(0, pos);
    };
    auto lower = header;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.rfind("cst:", 0) == 0) {
        session_.cst = header.substr(4);
        trim(session_.cst);
    }
    if (lower.rfind("x-security-token:", 0) == 0) {
        session_.security_token = header.substr(17);
        trim(session_.security_token);
    }
}

void CapitalClient::note_status(int status) {
    session_.last_http_status = status;
    if (status == 429) {
        ++session_.rate_limit_count;
        session_.cooldown_until = SteadyTimestamp(now_steady_ns().count() + kCooldown429Ns);
    }
    if (status == 401) {
        ++session_.auth_failure_count;
        session_.active = false;
        session_.cst.clear();
        session_.security_token.clear();
    }
}

bool CapitalClient::rate_limited() const {
    return now_steady_ns().count() < session_.cooldown_until.count();
}

CapitalHttpResult CapitalClient::http_request(
    const std::string& method, const std::string& path, const nlohmann::json& body, bool with_api_key) {
    CapitalHttpResult result;
    if (rate_limited()) {
        result.status = 429;
        return result;
    }

    CURL* curl = curl_easy_init();
    if (!curl) return result;

    std::string response;
    const std::string url = base_url_ + path;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    if (with_api_key && !session_.api_key.empty()) {
        headers = curl_slist_append(headers, ("X-CAP-API-KEY: " + session_.api_key).c_str());
    }
    if (!session_.cst.empty()) {
        headers = curl_slist_append(headers, ("CST: " + session_.cst).c_str());
        headers = curl_slist_append(headers, ("X-SECURITY-TOKEN: " + session_.security_token).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    std::string body_str;
    if (!body.empty()) {
        body_str = body.dump();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    }
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    const CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        result.status = 0;
        note_status(0);
        return result;
    }
    result.status = static_cast<int>(http_code);
    note_status(result.status);
    if (!response.empty()) {
        try {
            result.body = nlohmann::json::parse(response);
        } catch (...) {
            result.body = {};
        }
    }
    return result;
}

CapitalHttpResult CapitalClient::http_request_with_retry(
    const std::string& method, const std::string& path, const nlohmann::json& body) {
    CapitalHttpResult last;
    for (int attempt = 0; attempt < max_retries_; ++attempt) {
        if (rate_limited()) {
            last.status = 429;
            return last;
        }
        last = http_request(method, path, body, true);
        if (last.ok()) return last;
        if (last.status == 401) {
            if (!reconnect()) return last;
            continue;
        }
        if (last.status == 429) return last;
        if (last.status == 0 || last.status >= 500) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50 * (1 << attempt)));
            continue;
        }
        break;
    }
    return last;
}

bool CapitalClient::connect() {
    connected_ = true;
    return true;
}

void CapitalClient::disconnect() {
    connected_ = false;
    session_.active = false;
    session_.cst.clear();
    session_.security_token.clear();
}

HealthStatus CapitalClient::health() const {
    if (!connected_) return HealthStatus::Disconnected;
    if (rate_limited()) return HealthStatus::Degraded;
    if (!session_.active || session_.cst.empty()) return HealthStatus::Degraded;
    return HealthStatus::Healthy;
}

bool CapitalClient::authenticate(
    const std::string& api_key, const std::string& password, const std::string& identifier) {
    session_.api_key = api_key;
    session_.password = password;
    session_.identifier = identifier;
    session_.cst.clear();
    session_.security_token.clear();

    nlohmann::json body{{"identifier", identifier}, {"password", password}, {"encryptedPassword", false}};
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string response;
    const std::string url = base_url_ + "/api/v1/session";
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("X-CAP-API-KEY: " + api_key).c_str());
    const std::string body_str = body.dump();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    const CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    note_status(static_cast<int>(http_code));
    session_.active = (res == CURLE_OK && http_code >= 200 && http_code < 300 && !session_.cst.empty());
    connected_ = session_.active;
    if (!session_.active) ++session_.auth_failure_count;
    return connected_;
}

bool CapitalClient::reconnect() {
    if (session_.api_key.empty() || session_.password.empty() || session_.identifier.empty()) {
        return false;
    }
    ++session_.reconnect_count;
    return authenticate(session_.api_key, session_.password, session_.identifier);
}

std::optional<CapitalAccount> CapitalClient::account_info() {
    if (!connected_ && !session_.active) return std::nullopt;
    auto result = http_request_with_retry("GET", "/api/v1/accounts");
    if (!result.ok()) return std::nullopt;
    return parse_primary_account(result.body);
}

std::optional<double> CapitalClient::account_equity() {
    auto acc = account_info();
    if (!acc) return std::nullopt;
    if (acc->equity > 0.0) return acc->equity;
    if (acc->balance > 0.0) return acc->balance;
    return std::nullopt;
}

std::optional<CapitalQuote> CapitalClient::quote(InstrumentId instrument) {
    auto epic = instruments_.epic(instrument);
    if (!epic) return std::nullopt;
    auto result = http_request_with_retry("GET", "/api/v1/markets/" + *epic);
    if (!result.ok() || !result.body.contains("snapshot")) return std::nullopt;
    auto& snap = result.body["snapshot"];
    CapitalQuote q;
    q.bid = snap.value("bid", 0.0);
    q.ask = snap.value("offer", snap.value("ask", 0.0));
    q.ts = now_utc_ns();
    if (snap.contains("updateTime") && snap["updateTime"].is_string()) {
        nlohmann::json fake{{"snapshotTimeUTC", snap["updateTime"]}};
        auto parsed = parse_capital_timestamp(fake, q.ts);
        if (parsed.count() > 0) q.ts = parsed;
    }
    q.valid = q.bid > 0 && q.ask > 0;
    return q;
}

CapitalPriceHistory CapitalClient::prices(const std::string& epic, Timeframe tf, int max_bars) {
    CapitalPriceHistory out;
    if (epic.empty()) return out;
    if (static_cast<std::uint32_t>(tf) < static_cast<std::uint32_t>(Timeframe::Minute1)) {
        return out;  // Authority API is Minute1+ only
    }
    if (!connected_ && !session_.active) return out;

    const std::string resolution = capital_resolution(tf);
    const int capped = std::clamp(max_bars, 1, 100);
    std::ostringstream p1, p2, p3;
    p1 << "/api/v1/prices/" << epic << "?resolution=" << resolution << "&max=" << capped;
    p2 << "/api/v1/prices?epic=" << epic << "&resolution=" << resolution << "&max=" << capped;
    p3 << "/api/v1/history/prices?epic=" << epic << "&resolution=" << resolution << "&max=" << capped;

    CapitalHttpResult result;
    for (const auto& path : {p1.str(), p2.str(), p3.str()}) {
        result = http_request_with_retry("GET", path);
        if (result.ok()) break;
    }
    if (!result.ok()) return out;

    const nlohmann::json* arr = nullptr;
    if (result.body.contains("prices") && result.body["prices"].is_array()) {
        arr = &result.body["prices"];
    } else if (result.body.contains("candles") && result.body["candles"].is_array()) {
        arr = &result.body["candles"];
    }
    if (!arr) return out;

    const auto now = now_utc_ns();
    for (const auto& p : *arr) {
        auto bar = parse_price_bar(p, now);
        if (bar.close > 0) out.push_back(bar);
    }
    return out;
}

CapitalOrderResponse CapitalClient::create_position(const CapitalOrderRequest& request) {
    CapitalOrderResponse resp;
    if (!connected_) {
        resp.error_message = "not connected";
        return resp;
    }
    auto epic = instruments_.epic(request.instrument);
    if (!epic) {
        resp.error_message = "unknown instrument";
        return resp;
    }

    nlohmann::json body;
    body["epic"] = *epic;
    body["direction"] = request.direction == Direction::Long ? "BUY" : "SELL";
    body["size"] = request.quantity;
    if (request.stop_loss > 0) body["stopLevel"] = request.stop_loss;
    if (request.take_profit > 0) body["profitLevel"] = request.take_profit;

    auto result = http_request_with_retry("POST", "/api/v1/positions", body);
    if (!result.ok() || !result.body.contains("dealReference")) {
        resp.error_message = "rejected";
        return resp;
    }
    resp.success = true;
    resp.deal_id = result.body["dealReference"].get<std::string>();
    resp.fill_price = request.price;
    resp.filled_quantity = request.quantity;
    return resp;
}

CapitalOrderResponse CapitalClient::close_position(const std::string& deal_id) {
    CapitalOrderResponse resp;
    auto result = http_request_with_retry("DELETE", "/api/v1/positions/" + deal_id);
    resp.success = result.ok();
    resp.deal_id = deal_id;
    if (!resp.success) resp.error_message = "close failed";
    return resp;
}

std::vector<CapitalPosition> CapitalClient::positions() {
    std::vector<CapitalPosition> out;
    auto result = http_request_with_retry("GET", "/api/v1/positions");
    if (!result.ok() || !result.body.contains("positions")) return out;
    for (const auto& p : result.body["positions"]) {
        CapitalPosition pos;
        pos.deal_id = p.value("dealId", "");
        if (p.contains("position") && p["position"].is_object()) {
            const auto& posj = p["position"];
            pos.entry_price = posj.value("level", 0.0);
            pos.quantity = posj.value("size", 0.0);
            pos.direction = posj.value("direction", "") == "BUY" ? Direction::Long : Direction::Short;
        } else {
            pos.entry_price = p.value("level", 0.0);
            pos.quantity = p.value("size", 0.0);
            pos.direction = p.value("direction", "") == "BUY" ? Direction::Long : Direction::Short;
        }
        out.push_back(pos);
    }
    return out;
}

}  // namespace mr
