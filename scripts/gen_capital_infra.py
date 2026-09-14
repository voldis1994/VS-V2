#!/usr/bin/env python3
from pathlib import Path
import textwrap, shutil
ROOT = Path("/workspace")
VS = Path("/tmp/sources/VS")

def w(p, c):
    path = ROOT / p
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(textwrap.dedent(c).lstrip("\n"))

# capital-client
w("libs/capital-client/include/mr/capital/capital_quote.hpp", """
#pragma once
#include "mr/common/id.hpp"
namespace mr {
struct CapitalQuote { double bid{0}, ask{0}, Timestamp timestamp{}; bool valid{false}; };
}""")

w("libs/capital-client/include/mr/capital/capital_account.hpp", """
#pragma once
#include <string>
namespace mr {
struct CapitalAccount { std::string account_id; double balance{0}, available{0}; std::string currency; };
}""")

w("libs/capital-client/include/mr/capital/capital_instrument.hpp", """
#pragma once
#include "mr/common/id.hpp"
#include <string>
#include <unordered_map>
namespace mr {
class CapitalInstrumentMap {
public:
    void set(InstrumentId id, const std::string& epic);
    [[nodiscard]] std::optional<std::string> epic(InstrumentId id) const;
private:
    std::unordered_map<InstrumentId, std::string> map_;
};
}""")

w("libs/capital-client/include/mr/capital/capital_prices.hpp", """
#pragma once
#include "mr/capital/capital_quote.hpp"
#include <vector>
namespace mr {
struct CapitalPriceBar { Timestamp time{}; double open{0}, high{0}, low{0}, close{0}; };
using CapitalPriceHistory = std::vector<CapitalPriceBar>;
}""")

w("libs/capital-client/include/mr/capital/capital_session.hpp", """
#pragma once
#include <string>
namespace mr {
struct CapitalSession { std::string cst; std::string security_token; bool active{false}; };
}""")

w("libs/capital-client/include/mr/capital/capital_order.hpp", """
#pragma once
#include "mr/common/id.hpp"
#include <string>
namespace mr {
struct CapitalOrderRequest {
    InstrumentId instrument{kInvalidInstrument};
    Direction direction{Direction::Flat};
    double quantity{0}, price{0}, stop_loss{0}, take_profit{0};
};
struct CapitalOrderResponse {
    bool success{false}; std::string deal_id; double fill_price{0}, filled_quantity{0};
    std::string error_message;
};
struct CapitalPosition {
    std::string deal_id; Direction direction{Direction::Flat};
    double quantity{0}, entry_price{0}, unrealized_pnl{0};
};
}""")

w("libs/capital-client/include/mr/capital/capital_client.hpp", """
#pragma once
#include "mr/capital/capital_session.hpp"
#include "mr/capital/capital_account.hpp"
#include "mr/capital/capital_quote.hpp"
#include "mr/capital/capital_instrument.hpp"
#include "mr/capital/capital_order.hpp"
#include "mr/common/id.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>
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
}""")

# Adapt capital_client.cpp from VS
src = (VS / "libs/broker-adapters/src/capital_com_adapter.cpp").read_text()
src = src.replace("CapitalComAdapter", "CapitalClient")
src = src.replace("mr/broker_adapters/capital_com_adapter.hpp", "mr/capital/capital_client.hpp")
src = src.replace("BrokerAccountInfo", "CapitalAccount")
src = src.replace("BrokerQuote", "CapitalQuote")
src = src.replace("BrokerOrderRequest", "CapitalOrderRequest")
src = src.replace("BrokerOrderResponse", "CapitalOrderResponse")
src = src.replace("BrokerPosition", "CapitalPosition")
src = src.replace("epic_map_", "instruments_")
src = src.replace("epic_map_.find", "instruments_.epic")  # will need fix
# Rewrite capital_client.cpp properly
w("libs/capital-client/src/capital_client.cpp", """
#include "mr/capital/capital_client.hpp"
#include <sstream>
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
    if (header.rfind("CST:", 0) == 0) {
        session_.cst = header.substr(5);
        while (!session_.cst.empty() && (session_.cst.back()=='\r'||session_.cst.back()=='\n'||session_.cst.back()==' ')) session_.cst.pop_back();
    }
    if (header.rfind("X-SECURITY-TOKEN:", 0) == 0) {
        session_.security_token = header.substr(18);
        while (!session_.security_token.empty() && (session_.security_token.back()=='\r'||session_.security_token.back()=='\n'||session_.security_token.back()==' ')) session_.security_token.pop_back();
    }
}
nlohmann::json CapitalClient::http_request(const std::string& method, const std::string& path, const nlohmann::json& body) {
    CURL* curl = curl_easy_init(); if (!curl) return {};
    std::string response; std::string url = base_url_ + path;
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
    if (!body.empty()) { body_str = body.dump(); curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str()); }
    if (method == "POST") curl_easy_setopt(curl, CURLOPT_POST, 1L);
    else if (method == "DELETE") curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers); curl_easy_cleanup(curl);
    if (res != CURLE_OK) return {};
    try { return nlohmann::json::parse(response); } catch (...) { return {}; }
}
bool CapitalClient::connect() { connected_ = true; return true; }
void CapitalClient::disconnect() { connected_ = false; session_ = {}; }
HealthStatus CapitalClient::health() const {
    if (!connected_) return HealthStatus::Disconnected;
    if (!session_.active || session_.cst.empty()) return HealthStatus::Degraded;
    return HealthStatus::Healthy;
}
bool CapitalClient::authenticate(const std::string& api_key, const std::string& password, const std::string& identifier) {
    nlohmann::json body{{"identifier", identifier}, {"password", password}, {"encryptedPassword", false}};
    CURL* curl = curl_easy_init(); if (!curl) return false;
    std::string response; std::string url = base_url_ + "/api/v1/session";
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
    curl_slist_free_all(headers); curl_easy_cleanup(curl);
    session_.active = (res == CURLE_OK && !session_.cst.empty());
    connected_ = session_.active; return connected_;
}
std::optional<CapitalAccount> CapitalClient::account_info() {
    if (!connected_) return std::nullopt;
    auto json = http_request("GET", "/api/v1/accounts");
    if (json.empty() || !json.contains("accounts")) return std::nullopt;
    CapitalAccount info; auto& acc = json["accounts"][0];
    info.account_id = acc.value("accountId", ""); info.balance = acc.value("balance", 0.0);
    info.available = acc.value("available", 0.0); info.currency = acc.value("currency", "USD");
    return info;
}
std::optional<CapitalQuote> CapitalClient::quote(InstrumentId instrument) {
    auto epic = instruments_.epic(instrument); if (!epic) return std::nullopt;
    auto json = http_request("GET", "/api/v1/markets/" + *epic);
    if (json.empty() || !json.contains("snapshot")) return std::nullopt;
    auto& snap = json["snapshot"]; CapitalQuote q;
    q.bid = snap.value("bid", 0.0); q.ask = snap.value("offer", 0.0);
    q.timestamp = now_utc_ns(); q.valid = q.bid > 0 && q.ask > 0; return q;
}
CapitalOrderResponse CapitalClient::create_position(const CapitalOrderRequest& request) {
    CapitalOrderResponse resp; if (!connected_) { resp.error_message = "not connected"; return resp; }
    auto epic = instruments_.epic(request.instrument);
    if (!epic) { resp.error_message = "unknown instrument"; return resp; }
    nlohmann::json body; body["epic"] = *epic;
    body["direction"] = request.direction == Direction::Long ? "BUY" : "SELL";
    body["size"] = request.quantity;
    if (request.stop_loss > 0) body["stopLevel"] = request.stop_loss;
    if (request.take_profit > 0) body["profitLevel"] = request.take_profit;
    auto json = http_request("POST", "/api/v1/positions", body);
    if (json.empty() || !json.contains("dealReference")) { resp.error_message = "rejected"; return resp; }
    resp.success = true; resp.deal_id = json["dealReference"].get<std::string>();
    resp.fill_price = request.price; resp.filled_quantity = request.quantity; return resp;
}
CapitalOrderResponse CapitalClient::close_position(const std::string& deal_id) {
    CapitalOrderResponse resp; auto json = http_request("DELETE", "/api/v1/positions/" + deal_id);
    resp.success = !json.empty(); resp.deal_id = deal_id;
    if (!resp.success) resp.error_message = "close failed"; return resp;
}
std::vector<CapitalPosition> CapitalClient::positions() {
    std::vector<CapitalPosition> result;
    auto json = http_request("GET", "/api/v1/positions");
    if (json.empty() || !json.contains("positions")) return result;
    for (const auto& p : json["positions"]) {
        CapitalPosition pos; pos.deal_id = p.value("dealId", "");
        pos.entry_price = p.value("level", 0.0); pos.quantity = p.value("size", 0.0);
        pos.direction = p.value("direction", "") == "BUY" ? Direction::Long : Direction::Short;
        result.push_back(pos);
    }
    return result;
}
}""")

w("libs/capital-client/src/capital_instrument.cpp", """
#include "mr/capital/capital_instrument.hpp"
namespace mr {
void CapitalInstrumentMap::set(InstrumentId id, const std::string& epic) { map_[id] = epic; }
std::optional<std::string> CapitalInstrumentMap::epic(InstrumentId id) const {
    auto it = map_.find(id); return it == map_.end() ? std::nullopt : std::optional<std::string>(it->second);
}
}""")

w("libs/capital-client/src/capital_session.cpp", '#include "mr/capital/capital_session.hpp"\n')

# paper broker adapted
w("libs/capital-client/src/paper_broker.cpp", """
#include "mr/capital/capital_client.hpp"
#include <unordered_map>
namespace mr {
class PaperCapitalClient {
public:
    bool connected{false};
    CapitalAccount account{"paper", 100000, 100000, "USD"};
    std::unordered_map<InstrumentId, CapitalQuote> quotes;
    std::vector<CapitalPosition> open;
    std::uint64_t deal_counter{1};
};
}""")

w("libs/capital-client/CMakeLists.txt", """
add_library(mr_capital_client STATIC
    src/capital_client.cpp src/capital_instrument.cpp src/capital_session.cpp src/paper_broker.cpp)
add_library(mr::capital-client ALIAS mr_capital_client)
target_include_directories(mr_capital_client PUBLIC include)
target_link_libraries(mr_capital_client PUBLIC mr::common mr::market-types CURL::libcurl nlohmann_json::nlohmann_json)
target_compile_features(mr_capital_client PUBLIC cxx_std_20)
""")

# execution-engine
w("libs/execution-engine/include/mr/execution_engine/order_builder.hpp", """
#pragma once
#include "mr/capital/capital_order.hpp"
#include "mr/decision_engine/decision_types.hpp"
namespace mr {
class OrderBuilder {
public:
    CapitalOrderRequest from_intent(const TradeIntent& intent, double quantity) const;
};
}""")

w("libs/execution-engine/include/mr/execution_engine/capital_executor.hpp", """
#pragma once
#include "mr/capital/capital_client.hpp"
#include "mr/execution_engine/order_builder.hpp"
namespace mr {
class CapitalExecutor {
public:
    explicit CapitalExecutor(CapitalClient& client) : client_(client) {}
    CapitalOrderResponse execute(const CapitalOrderRequest& req);
    CapitalOrderResponse close(const std::string& deal_id);
private:
    CapitalClient& client_;
};
}""")

w("libs/execution-engine/include/mr/execution_engine/fill_tracker.hpp", """
#pragma once
#include "mr/capital/capital_order.hpp"
#include <vector>
namespace mr {
struct FillRecord { CapitalOrderResponse response; Timestamp ts{}; };
class FillTracker {
public:
    void record(const FillRecord& f);
    [[nodiscard]] const std::vector<FillRecord>& fills() const { return fills_; }
private:
    std::vector<FillRecord> fills_;
};
}""")

w("libs/execution-engine/include/mr/execution_engine/retry_policy.hpp", """
#pragma once
namespace mr {
struct RetryPolicy { std::uint32_t max_attempts{3}; std::uint64_t backoff_ms{100}; };
}""")

w("libs/execution-engine/include/mr/execution_engine/reconciliation.hpp", """
#pragma once
#include "mr/capital/capital_client.hpp"
#include "mr/execution_engine/fill_tracker.hpp"
namespace mr {
class Reconciliation {
public:
    Reconciliation(CapitalClient& c, FillTracker& t) : client_(c), tracker_(t) {}
    bool reconcile();
private:
    CapitalClient& client_; FillTracker& tracker_;
};
}""")

w("libs/execution-engine/include/mr/execution_engine/execution_engine.hpp", """
#pragma once
#include "mr/execution_engine/capital_executor.hpp"
#include "mr/execution_engine/fill_tracker.hpp"
#include "mr/execution_engine/retry_policy.hpp"
#include "mr/execution_engine/reconciliation.hpp"
#include "mr/risk_engine/risk_engine.hpp"
namespace mr {
class ExecutionEngine {
public:
    ExecutionEngine(CapitalClient& client, RiskEngine& risk);
    CapitalOrderResponse submit(const TradeIntent& intent, double quantity);
private:
    CapitalClient& client_; RiskEngine& risk_;
    OrderBuilder builder_; CapitalExecutor executor_; FillTracker fills_;
    RetryPolicy retry_; Reconciliation recon_;
};
}""")

w("libs/execution-engine/src/order_builder.cpp", """
#include "mr/execution_engine/order_builder.hpp"
namespace mr {
CapitalOrderRequest OrderBuilder::from_intent(const TradeIntent& intent, double quantity) const {
    CapitalOrderRequest r; r.instrument = intent.instrument; r.direction = intent.direction;
    r.quantity = quantity; r.price = intent.reference_price;
    r.stop_loss = intent.stop_loss; r.take_profit = intent.take_profit; return r;
}
}""")

w("libs/execution-engine/src/capital_executor.cpp", """
#include "mr/execution_engine/capital_executor.hpp"
namespace mr {
CapitalOrderResponse CapitalExecutor::execute(const CapitalOrderRequest& req) { return client_.create_position(req); }
CapitalOrderResponse CapitalExecutor::close(const std::string& deal_id) { return client_.close_position(deal_id); }
}""")

w("libs/execution-engine/src/fill_tracker.cpp", """
#include "mr/execution_engine/fill_tracker.hpp"
namespace mr {
void FillTracker::record(const FillRecord& f) { fills_.push_back(f); }
}""")

w("libs/execution-engine/src/reconciliation.cpp", """
#include "mr/execution_engine/reconciliation.hpp"
namespace mr {
bool Reconciliation::reconcile() {
    auto positions = client_.positions();
    return !positions.empty() || tracker_.fills().empty();
}
}""")

w("libs/execution-engine/src/execution_engine.cpp", """
#include "mr/execution_engine/execution_engine.hpp"
#include "mr/common/clock.hpp"
namespace mr {
ExecutionEngine::ExecutionEngine(CapitalClient& client, RiskEngine& risk)
    : client_(client), risk_(risk), executor_(client), recon_(client, fills_) {}
CapitalOrderResponse ExecutionEngine::submit(const TradeIntent& intent, double quantity) {
    auto guard = risk_.pre_trade_check(intent, 0);
    if (!guard.pass) { CapitalOrderResponse r; r.error_message = guard.reason; return r; }
    auto req = builder_.from_intent(intent, quantity);
    CapitalOrderResponse resp;
    for (std::uint32_t i = 0; i < retry_.max_attempts; ++i) {
        resp = executor_.execute(req);
        if (resp.success) { fills_.record({resp, now_utc_ns()}); recon_.reconcile(); return resp; }
    }
    return resp;
}
}""")

w("libs/execution-engine/CMakeLists.txt", """
add_library(mr_execution_engine STATIC
    src/order_builder.cpp src/capital_executor.cpp src/fill_tracker.cpp
    src/reconciliation.cpp src/execution_engine.cpp)
add_library(mr::execution-engine ALIAS mr_execution_engine)
target_include_directories(mr_execution_engine PUBLIC include)
target_link_libraries(mr_execution_engine PUBLIC mr::capital-client mr::risk-engine mr::decision-engine)
target_compile_features(mr_execution_engine PUBLIC cxx_std_20)
""")

# telemetry - adapt from workspace
tel_h = (ROOT / "libs/telemetry/include/mr/telemetry/telemetry_hub.hpp").read_text()
tel_h = tel_h.replace('mr/common/types.hpp', 'mr/common/id.hpp')
tel_h = tel_h.replace('mr/clock/clock_engine.hpp', 'mr/common/clock.hpp')
tel_h = tel_h.replace('ClockEngine', 'Clock')
(ROOT / "libs/telemetry/include/mr/telemetry/telemetry_hub.hpp").write_text(tel_h)

tel_cpp = (ROOT / "libs/telemetry/src/telemetry_hub.cpp").read_text()
tel_cpp = tel_cpp.replace('ClockEngine', 'Clock')
(ROOT / "libs/telemetry/src/telemetry_hub.cpp").write_text(tel_cpp)

w("libs/telemetry/CMakeLists.txt", """
add_library(mr_telemetry STATIC src/telemetry_hub.cpp)
add_library(mr::telemetry ALIAS mr_telemetry)
target_include_directories(mr_telemetry PUBLIC include)
target_link_libraries(mr_telemetry PUBLIC mr::common nlohmann_json::nlohmann_json)
target_compile_features(mr_telemetry PUBLIC cxx_std_20)
""")

# persistence
w("libs/persistence/include/mr/persistence/persistence.hpp", """
#pragma once
#include "mr/market_types/market_event.hpp"
#include <string>
namespace mr {
class IPersistence {
public:
    virtual ~IPersistence() = default;
    virtual void store(const MarketEvent& e) = 0;
    virtual void flush() = 0;
};
}""")

w("libs/persistence/include/mr/persistence/repository.hpp", """
#pragma once
#include "mr/persistence/persistence.hpp"
#include <vector>
namespace mr {
class EventRepository : public IPersistence {
public:
    explicit EventRepository(const std::string& path);
    void store(const MarketEvent& e) override;
    void flush() override;
    [[nodiscard]] std::uint64_t count() const { return count_; }
private:
    std::string path_;
    std::uint64_t count_{0};
};
}""")

w("libs/persistence/include/mr/persistence/transaction.hpp", """
#pragma once
#include "mr/persistence/persistence.hpp"
#include <vector>
namespace mr {
class Transaction {
public:
    explicit Transaction(IPersistence& backend) : backend_(backend) {}
    void append(const MarketEvent& e) { batch_.push_back(e); }
    void commit();
    void rollback() { batch_.clear(); }
private:
    IPersistence& backend_;
    std::vector<MarketEvent> batch_;
};
}""")

# adapt raw_event_storage
raw_h = (ROOT / "libs/persistence/include/mr/persistence/raw_event_storage.hpp").read_text()
raw_h = raw_h.replace('mr/common/market_event.hpp', 'mr/market_types/market_event.hpp')
(ROOT / "libs/persistence/include/mr/persistence/raw_event_storage.hpp").write_text(raw_h)

w("libs/persistence/src/repository.cpp", """
#include "mr/persistence/repository.hpp"
#include "mr/persistence/raw_event_storage.hpp"
namespace mr {
EventRepository::EventRepository(const std::string& path) : path_(path) {}
void EventRepository::store(const MarketEvent& e) { RawEventWriter w(path_); w.write(e); count_++; }
void EventRepository::flush() {}
}""")

w("libs/persistence/src/transaction.cpp", """
#include "mr/persistence/transaction.hpp"
namespace mr {
void Transaction::commit() { for (auto& e : batch_) backend_.store(e); batch_.clear(); }
}""")

w("libs/persistence/CMakeLists.txt", """
add_library(mr_persistence STATIC
    src/raw_event_storage.cpp src/repository.cpp src/transaction.cpp)
add_library(mr::persistence ALIAS mr_persistence)
target_include_directories(mr_persistence PUBLIC include)
target_link_libraries(mr_persistence PUBLIC mr::market-types mr::common)
target_compile_features(mr_persistence PUBLIC cxx_std_20)
""")

# replay lib
replay_h = (VS / "libs/replay/include/mr/replay/replay_engine.hpp").read_text()
replay_h = replay_h.replace('mr/common/market_event.hpp', 'mr/market_types/market_event.hpp')
replay_h = replay_h.replace('mr/common/types.hpp', 'mr/common/id.hpp')
w("libs/replay/include/mr/replay/replay_engine.hpp", replay_h)

replay_cpp = (VS / "libs/replay/src/replay_engine.cpp").read_text()
replay_cpp = replay_cpp.replace('mr/common/market_event.hpp', 'mr/market_types/market_event.hpp')
w("libs/replay/src/replay_engine.cpp", replay_cpp)

w("libs/replay/CMakeLists.txt", """
add_library(mr_replay STATIC src/replay_engine.cpp)
add_library(mr::replay ALIAS mr_replay)
target_include_directories(mr_replay PUBLIC include)
target_link_libraries(mr_replay PUBLIC mr::market-types mr::persistence)
target_compile_features(mr_replay PUBLIC cxx_std_20)
""")

# market-core app
w("apps/market-core/include/mr/market_core/pipeline.hpp", """
#pragma once
#include "mr/brain_core/market_brain.hpp"
#include "mr/normalization/normalizer.hpp"
#include "mr/data_quality/quality_engine.hpp"
#include "mr/feed_fusion/feed_fusion_engine.hpp"
#include "mr/perception_engine/perception_engine.hpp"
#include "mr/structure_engine/structure_engine.hpp"
#include "mr/market_concepts/market_concepts_engine.hpp"
#include "mr/microstructure_engine/microstructure_engine.hpp"
#include "mr/scenario_engine/scenario_engine.hpp"
#include "mr/prediction_engine/prediction_engine.hpp"
#include "mr/decision_engine/decision_engine.hpp"
#include "mr/telemetry/telemetry_hub.hpp"
#include "mr/common/config.hpp"
#include <vector>
namespace mr {
class MarketCorePipeline {
public:
    MarketCorePipeline();
    void configure(const ConfigRegistry& config);
    void process_event(const MarketEvent& event);
    [[nodiscard]] std::vector<TradeIntent> pending_intents() const { return pending_; }
    std::vector<TradeIntent> drain_pending_intents();
    [[nodiscard]] TelemetryHub& telemetry() { return telemetry_; }
private:
    SystemClock clock_;
    Normalizer normalizer_;
    QualityEngine quality_;
    FeedFusionEngine fusion_;
    MarketBrain brain_;
    PerceptionEngineFacade perception_;
    StructureEngine structure_;
    MarketConceptsEngine concepts_;
    MicrostructureEngine micro_;
    ScenarioEngine scenarios_;
    PredictionEngine prediction_;
    DecisionEngine decision_;
    TelemetryHub telemetry_;
    IdGenerator intent_ids_;
    std::vector<TradeIntent> pending_;
    double stale_ms_{500};
};
}""")

w("apps/market-core/src/pipeline.cpp", """
#include "mr/market_core/pipeline.hpp"
namespace mr {
MarketCorePipeline::MarketCorePipeline() : normalizer_(clock_), decision_(intent_ids_) {}
void MarketCorePipeline::configure(const ConfigRegistry& config) {
    stale_ms_ = config.get_double("stale_threshold_ms", 500.0);
}
void MarketCorePipeline::process_event(const MarketEvent& event) {
    telemetry_.record_event();
    auto norm = normalizer_.normalize(event);
    quality_.process(norm, stale_ms_);
    auto health = quality_.health(norm.source);
    fusion_.ingest(norm, health);
    auto consensus = fusion_.consensus(norm.instrument);
    brain_.on_normalized(norm, consensus);
    perception_.on_event(norm, consensus.mid);
    auto pd = perception_.dynamics();
    structure_.update(consensus.mid, pd, brain_.candles(norm.instrument).state());
    auto st = structure_.snapshot();
    auto rg = concepts_.evaluate(pd, st);
    micro_.update(norm);
    auto scens = scenarios_.evaluate(st, rg, micro_.snapshot());
    if (scens.empty()) return;
    auto pred = prediction_.predict(scens[0], pd, Direction::Long);
    auto opp = decision_.evaluate_opportunity(scens[0], pred, consensus.spread, Direction::Long);
    Quote q; q.instrument = norm.instrument; q.spread.bid = consensus.mid - consensus.spread/2;
    q.spread.ask = consensus.mid + consensus.spread/2; q.valid = consensus.valid();
    auto intent = decision_.decide(opp, q);
    if (intent.decision == EntryDecision::EntryReady) pending_.push_back(intent);
    telemetry_.record_decision();
}
std::vector<TradeIntent> MarketCorePipeline::drain_pending_intents() {
    auto out = pending_; pending_.clear(); return out;
}
}""")

w("apps/market-core/src/main.cpp", """
#include "mr/market_core/pipeline.hpp"
#include <iostream>
int main() {
    mr::MarketCorePipeline pipeline;
    std::cout << "market-core ready (" << mr::kBrainVersion << ")" << std::endl;
    return 0;
}""")

w("apps/market-core/CMakeLists.txt", """
add_executable(market-core apps/market-core/src/main.cpp apps/market-core/src/pipeline.cpp)
target_include_directories(market-core PRIVATE apps/market-core/include)
target_link_libraries(market-core PRIVATE
    mr::brain-core mr::normalization mr::data-quality mr::feed-fusion
    mr::perception-engine mr::structure-engine mr::market-concepts
    mr::microstructure-engine mr::scenario-engine mr::prediction-engine
    mr::decision-engine mr::telemetry mr::common)
target_compile_features(market-core PRIVATE cxx_std_20)
""")

# root cmake + vcpkg + tests
w("CMakeLists.txt", """
cmake_minimum_required(VERSION 3.24)
project(vs-v2 VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MR_BUILD_TESTS "Build tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
include(Dependencies)

add_subdirectory(libs/common)
add_subdirectory(libs/market-types)
add_subdirectory(libs/market-data)
add_subdirectory(libs/capital-client)
add_subdirectory(libs/normalization)
add_subdirectory(libs/data-quality)
add_subdirectory(libs/feed-fusion)
add_subdirectory(libs/candle-engine)
add_subdirectory(libs/brain-core)
add_subdirectory(libs/perception-engine)
add_subdirectory(libs/structure-engine)
add_subdirectory(libs/market-concepts)
add_subdirectory(libs/microstructure-engine)
add_subdirectory(libs/cross-market-engine)
add_subdirectory(libs/memory-engine)
add_subdirectory(libs/pattern-engine)
add_subdirectory(libs/scenario-engine)
add_subdirectory(libs/prediction-engine)
add_subdirectory(libs/decision-engine)
add_subdirectory(libs/position-brain)
add_subdirectory(libs/risk-engine)
add_subdirectory(libs/execution-engine)
add_subdirectory(libs/telemetry)
add_subdirectory(libs/persistence)
add_subdirectory(libs/replay)
add_subdirectory(apps/market-core)

if(MR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
""")

w("requirements/cpp/vcpkg.json", """{
  "name": "vs-v2",
  "version-string": "2.0.0",
  "dependencies": [
    "fmt",
    "spdlog",
    "yaml-cpp",
    "nlohmann-json",
    "openssl",
    "curl",
    "gtest",
    "zlib"
  ],
  "features": {
    "benchmarks": {
      "description": "Google Benchmark (optional)",
      "dependencies": ["benchmark"]
    }
  },
  "builtin-baseline": "3508985146f1b1d248c67ead13f8f54be5b4f5da"
}
""")

w("tests/CMakeLists.txt", """
add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(replay)
""")

# Fix apps/market-core cmake path
apps_cmake = (ROOT / "apps/market-core/CMakeLists.txt").read_text()
apps_cmake = apps_cmake.replace("apps/market-core/src/", "src/")
(ROOT / "apps/market-core/CMakeLists.txt").write_text(apps_cmake)

# Remove old broker_adapter files
for f in ["libs/capital-client/include/mr/capital/broker_adapter.hpp"]:
    p = ROOT / f
    if p.exists(): p.unlink()

print("capital + infra done")
