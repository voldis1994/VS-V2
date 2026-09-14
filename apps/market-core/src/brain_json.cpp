#include "mr/market_core/brain_json.hpp"

#include <curl/curl.h>

namespace mr {
namespace {

const char* action_str(TradeAction a) {
    switch (a) {
        case TradeAction::Buy:
            return "BUY";
        case TradeAction::Sell:
            return "SELL";
        case TradeAction::Wait:
        default:
            return "WAIT";
    }
}

const char* dir_str(Direction d) {
    switch (d) {
        case Direction::Long:
            return "LONG";
        case Direction::Short:
            return "SHORT";
        case Direction::Flat:
        default:
            return "FLAT";
    }
}

const char* trend_str(TrendBias b) {
    switch (b) {
        case TrendBias::Up:
            return "Up";
        case TrendBias::Down:
            return "Down";
        case TrendBias::Range:
            return "Range";
        case TrendBias::Unknown:
        default:
            return "Unknown";
    }
}

const char* exec_status_str(ExecutionStatus s) {
    switch (s) {
        case ExecutionStatus::Submitted:
            return "SUBMITTED";
        case ExecutionStatus::Filled:
            return "FILLED";
        case ExecutionStatus::Rejected:
            return "REJECTED";
        case ExecutionStatus::Retrying:
            return "RETRYING";
        case ExecutionStatus::Idle:
        default:
            return "NONE";
    }
}

const char* pos_action_str(PositionAction a) {
    switch (a) {
        case PositionAction::Protect:
            return "PROTECT";
        case PositionAction::Reduce:
            return "REDUCE";
        case PositionAction::Exit:
            return "EXIT";
        case PositionAction::Hold:
        default:
            return "HOLD";
    }
}


nlohmann::json opt_num(const std::optional<double>& v) {
    return v.has_value() ? nlohmann::json(*v) : nlohmann::json(nullptr);
}

std::string health_or_unknown(const std::string& s) {
    return s.empty() ? std::string{"UNKNOWN"} : s;
}

nlohmann::json side_to_json(const SidePrediction& s) {
    return {
        {"direction", dir_str(s.direction)},
        {"continuation", s.continuation},
        {"reversal_failure", s.reversal_failure},
        {"expected_move", s.expected_move},
        {"adverse_move", s.adverse_move},
        {"probability", s.probability},
        {"confidence", s.confidence},
        {"expected_value", s.expected_value},
        {"invalidation", s.invalidation},
        {"thesis_quality", s.thesis_quality},
        {"uncertainty", s.uncertainty},
    };
}

nlohmann::json instrument_to_json(InstrumentId id, const BrainContext& ctx,
                                  const BrainFeedRuntime& runtime) {
    const double mid = ctx.consensus.mid;
    const double spread = ctx.consensus.spread;
    const double half = spread > 0.0 ? spread * 0.5 : 0.0;

    return {
        {"instrument_id", static_cast<std::uint64_t>(id)},
        {"quote",
         {{"bid", mid - half},
          {"ask", mid + half},
          {"mid", mid},
          {"spread", spread},
          {"ts_ns", ctx.ts.count()}}},
        {"structure",
         {{"swing_state", ctx.structure.swing_state},
          {"trend_direction", trend_str(ctx.structure.trend_direction)},
          {"trend_strength", ctx.structure.trend_strength},
          {"volatility", ctx.structure.volatility},
          {"structure_quality", ctx.structure.structure_quality},
          {"structural_invalidation", ctx.structure.structural_invalidation},
          {"compression", ctx.structure.compression},
          {"expansion", ctx.structure.expansion},
          {"breakout_active", ctx.structure.breakout_active},
          {"in_range", ctx.structure.in_range},
          {"range_position", ctx.structure.range_position}}},
        {"micro",
         {{"spread", ctx.micro.spread},
          {"momentum", ctx.micro.momentum},
          {"acceleration", ctx.micro.acceleration},
          {"acceptance", ctx.micro.acceptance},
          {"rejection", ctx.micro.rejection},
          {"reclaim", ctx.micro.reclaim},
          {"body_pct", ctx.micro.body_pct},
          {"candle_strength", ctx.micro.candle_strength},
          {"swing_state", ctx.micro.swing_state},
          {"aggressive_buy_pressure", ctx.micro.aggressive_buy_pressure},
          {"aggressive_sell_pressure", ctx.micro.aggressive_sell_pressure}}},
        {"prediction",
         {{"long_side", side_to_json(ctx.prediction.long_side)},
          {"short_side", side_to_json(ctx.prediction.short_side)},
          {"has_structure_authority", ctx.prediction.has_structure_authority},
          {"has_micro_authority", ctx.prediction.has_micro_authority},
          {"evidence_sufficient", ctx.prediction.evidence_sufficient},
          {"structure_volatility", ctx.prediction.structure_volatility},
          {"structure_invalidation", ctx.prediction.structure_invalidation}}},
        {"decision",
         {{"instrument_id", static_cast<std::uint64_t>(ctx.decision.instrument)},
          {"direction", dir_str(ctx.decision.direction)},
          {"probability", ctx.decision.probability},
          {"expected_value", ctx.decision.expected_value},
          {"spread_cost", ctx.decision.spread_cost},
          {"action", action_str(ctx.decision.action)},
          {"reason_codes", ctx.decision.reason_codes},
          {"stop_distance_frac", ctx.decision.stop_distance_frac},
          {"target_distance_frac", ctx.decision.target_distance_frac}}},
        {"decision_action", action_str(ctx.decision_action)},
        {"risk",
         {{"approved", ctx.risk.approved},
          {"approved_quantity", ctx.risk.approved_quantity},
          {"reason_codes", ctx.risk.reason_codes},
          {"exposure", opt_num(runtime.exposure)},
          {"daily_pnl", opt_num(runtime.daily_pnl)},
          {"max_drawdown", opt_num(runtime.max_drawdown)},
          {"risk_budget_used", ctx.risk.size_fraction}}},
        {"execution",
         {{"status", exec_status_str(ctx.execution.status)},
          {"deal_id", ctx.execution.deal_id},
          {"fill_price", ctx.execution.fill_price},
          {"filled_quantity", ctx.execution.filled_quantity},
          {"message", ctx.execution.explanation}}},
        {"position",
         {{"instrument_id", static_cast<std::uint64_t>(ctx.position.instrument)},
          {"direction", dir_str(ctx.position.direction)},
          {"quantity", ctx.position.quantity},
          {"entry_price", ctx.position.entry_price},
          {"current_price", ctx.position.current_price},
          {"unrealized_pnl", ctx.position.current_pnl},
          {"realized_pnl", opt_num(runtime.realized_pnl)},
          {"mfe", ctx.position.mfe},
          {"mae", ctx.position.mae},
          {"deal_id", ctx.position.deal_id},
          {"position_action", pos_action_str(ctx.position_decision.action)}}},
        {"has_structure_authority", ctx.has_structure_authority},
        {"has_micro_authority", ctx.has_micro_authority},
        {"has_prediction", ctx.has_prediction},
        {"has_decision", ctx.has_decision},
        {"has_risk", ctx.has_risk},
        {"has_execution", ctx.has_execution},
        {"has_position", ctx.has_position},
        {"ts_ns", ctx.ts.count()},
    };
}

size_t sink(char*, size_t size, size_t nmemb, void*) { return size * nmemb; }

struct CurlBody {
    std::string data;
};

size_t write_body(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<CurlBody*>(userdata);
    body->data.append(ptr, size * nmemb);
    return size * nmemb;
}

}  // namespace

nlohmann::json brain_snapshot_to_json(const BrainSnapshot& snap,
                                      const std::string& model_id,
                                      const std::string& model_version,
                                      const std::string& operating_mode,
                                      const BrainFeedRuntime& runtime) {
    nlohmann::json instruments = nlohmann::json::array();
    for (const auto& [id, ctx] : snap.instruments) {
        instruments.push_back(instrument_to_json(id, ctx, runtime));
    }
    const std::string mode = operating_mode.empty() ? "UNKNOWN" : operating_mode;
    return {
        {"source", "market-core"},
        {"brain_version", kBrainVersion},
        {"model_id", model_id},
        {"model_version", model_version},
        {"snapshot_id", static_cast<std::uint64_t>(snap.id)},
        {"ts_ns", snap.ts.count()},
        {"operating_mode", mode},
        {"health",
         {{"market_core", health_or_unknown(runtime.market_core_health)},
          {"feeds", health_or_unknown(runtime.feeds_health)},
          {"execution", health_or_unknown(runtime.execution_health)},
          {"data", health_or_unknown(runtime.data_health)}}},
        {"instruments", std::move(instruments)},
    };
}

int publish_brain_snapshot_to_control_api(const nlohmann::json& body,
                                          const std::string& control_api_url,
                                          const std::string& pipeline_token) {
    if (control_api_url.empty()) return -1;
    const std::string url = control_api_url + "/api/pipeline/brain-snapshot";
    const std::string payload = body.dump();

    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!pipeline_token.empty()) {
        const std::string auth = "x-pipeline-token: " + pipeline_token;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sink);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);

    const CURLcode rc = curl_easy_perform(curl);
    long http = -1;
    if (rc == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return static_cast<int>(http);
}


int publish_pipeline_heartbeat_to_control_api(const nlohmann::json& body,
                                              const std::string& control_api_url,
                                              const std::string& pipeline_token) {
    if (control_api_url.empty()) return -1;
    const std::string url = control_api_url + "/api/pipeline/heartbeat";
    const std::string payload = body.dump();

    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!pipeline_token.empty()) {
        const std::string auth = "x-pipeline-token: " + pipeline_token;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sink);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);

    const CURLcode rc = curl_easy_perform(curl);
    long http = -1;
    if (rc == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return static_cast<int>(http);
}

std::optional<std::string> fetch_requested_runtime_mode_from_control_api(
    const std::string& control_api_url,
    const std::string& pipeline_token) {
    if (control_api_url.empty()) return std::nullopt;
    const std::string url = control_api_url + "/api/system/runtime-mode";

    CURL* curl = curl_easy_init();
    if (!curl) return std::nullopt;

    CurlBody body;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    if (!pipeline_token.empty()) {
        const std::string auth = "x-pipeline-token: " + pipeline_token;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);

    const CURLcode rc = curl_easy_perform(curl);
    long http = -1;
    if (rc == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (http != 200 || body.data.empty()) return std::nullopt;
    try {
        const auto j = nlohmann::json::parse(body.data);
        if (!j.contains("mode") || !j["mode"].is_string()) return std::nullopt;
        const std::string mode = j["mode"].get<std::string>();
        if (mode.empty()) return std::nullopt;
        return mode;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace mr
