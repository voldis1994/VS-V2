#include "mr/capital/capital_client.hpp"
#include "mr/common/clock.hpp"
#include <algorithm>

namespace mr {

CapitalClient::CapitalClient(const std::string& base_url) : base_url_(base_url) {}
CapitalClient::~CapitalClient() = default;

size_t CapitalClient::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

size_t CapitalClient::header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    static_cast<CapitalClient*>(userdata)->apply_auth_header(std::string(buffer, size * nitems));
    return size * nitems;
}

void CapitalClient::apply_auth_header(const std::string& header) {
    auto trim = [](std::string& s) {
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ')) {
            s.pop_back();
        }
    };
    if (header.rfind("CST:", 0) == 0) {
        session_.cst = header.substr(5);
        trim(session_.cst);
    }
    if (header.rfind("X-SECURITY-TOKEN:", 0) == 0) {
        session_.security_token = header.substr(18);
        trim(session_.security_token);
    }
}

nlohmann::json CapitalClient::http_request(
    const std::string& method, const std::string& path, const nlohmann::json& body) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};

    std::string response;
    std::string url = base_url_ + path;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!session_.cst.empty()) {
        headers = curl_slist_append(headers, ("CST: " + session_.cst).c_str());
        headers = curl_slist_append(headers, ("X-SECURITY-TOKEN: " + session_.security_token).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    std::string body_str;
    if (!body.empty()) {
        body_str = body.dump();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    }
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) return {};
    try {
        return nlohmann::json::parse(response);
    } catch (...) {
        return {};
    }
}

bool CapitalClient::connect() {
    connected_ = true;
    return true;
}

void CapitalClient::disconnect() {
    connected_ = false;
    session_ = {};
}

HealthStatus CapitalClient::health() const {
    if (!connected_) return HealthStatus::Disconnected;
    if (!session_.active || session_.cst.empty()) return HealthStatus::Degraded;
    return HealthStatus::Healthy;
}

bool CapitalClient::authenticate(
    const std::string& api_key, const std::string& password, const std::string& identifier) {
    nlohmann::json body{{"identifier", identifier}, {"password", password}, {"encryptedPassword", false}};
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string response;
    std::string url = base_url_ + "/api/v1/session";
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("X-CAP-API-KEY: " + api_key).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.dump().c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    session_.active = (res == CURLE_OK && !session_.cst.empty());
    connected_ = session_.active;
    return connected_;
}

std::optional<CapitalAccount> CapitalClient::account_info() {
    if (!connected_) return std::nullopt;
    auto json = http_request("GET", "/api/v1/accounts");
    if (json.empty() || !json.contains("accounts")) return std::nullopt;
    CapitalAccount info;
    auto& acc = json["accounts"][0];
    info.account_id = acc.value("accountId", "");
    info.balance = acc.value("balance", 0.0);
    info.available = acc.value("available", 0.0);
    info.currency = acc.value("currency", "USD");
    return info;
}

std::optional<CapitalQuote> CapitalClient::quote(InstrumentId instrument) {
    auto epic = instruments_.epic(instrument);
    if (!epic) return std::nullopt;
    auto json = http_request("GET", "/api/v1/markets/" + *epic);
    if (json.empty() || !json.contains("snapshot")) return std::nullopt;
    auto& snap = json["snapshot"];
    CapitalQuote q;
    q.bid = snap.value("bid", 0.0);
    q.ask = snap.value("offer", 0.0);
    q.ts = now_utc_ns();
    q.valid = q.bid > 0 && q.ask > 0;
    return q;
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

    auto json = http_request("POST", "/api/v1/positions", body);
    if (json.empty() || !json.contains("dealReference")) {
        resp.error_message = "rejected";
        return resp;
    }
    resp.success = true;
    resp.deal_id = json["dealReference"].get<std::string>();
    resp.fill_price = request.price;
    resp.filled_quantity = request.quantity;
    return resp;
}

CapitalOrderResponse CapitalClient::close_position(const std::string& deal_id) {
    CapitalOrderResponse resp;
    auto json = http_request("DELETE", "/api/v1/positions/" + deal_id);
    resp.success = !json.empty();
    resp.deal_id = deal_id;
    if (!resp.success) resp.error_message = "close failed";
    return resp;
}

std::vector<CapitalPosition> CapitalClient::positions() {
    std::vector<CapitalPosition> result;
    auto json = http_request("GET", "/api/v1/positions");
    if (json.empty() || !json.contains("positions")) return result;
    for (const auto& p : json["positions"]) {
        CapitalPosition pos;
        pos.deal_id = p.value("dealId", "");
        pos.entry_price = p.value("level", 0.0);
        pos.quantity = p.value("size", 0.0);
        pos.direction = p.value("direction", "") == "BUY" ? Direction::Long : Direction::Short;
        result.push_back(pos);
    }
    return result;
}

}  // namespace mr
