#include "mr/market_core/brain_runtime.hpp"
#include "mr/market_core/brain_json.hpp"
#include <nlohmann/json.hpp>

#include <cstdlib>

namespace mr {
namespace {

const char* mode_str(OperatingMode m) {
    switch (m) {
        case OperatingMode::Replay:
            return "REPLAY";
        case OperatingMode::Paper:
            return "PAPER";
        case OperatingMode::Demo:
            return "DEMO";
        case OperatingMode::Shadow:
            return "SHADOW";
        case OperatingMode::Live:
            return "LIVE";
        default:
            return "UNKNOWN";
    }
}

}  // namespace

void BrainRuntime::configure_from_env() {
    if (const char* url = std::getenv("CONTROL_API_URL")) {
        control_api_url_ = url;
    }
    if (const char* tok = std::getenv("PIPELINE_TOKEN")) {
        pipeline_token_ = tok;
    } else if (const char* tok = std::getenv("PIPELINE_SERVICE_TOKEN")) {
        pipeline_token_ = tok;
    }
}

std::optional<OperatingMode> BrainRuntime::pull_requested_operating_mode() const {
    auto raw = fetch_requested_runtime_mode_from_control_api(control_api_url_, pipeline_token_);
    if (!raw) return std::nullopt;
    std::string m = *raw;
    for (auto& c : m) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    if (m == "LIVE") return OperatingMode::Live;
    if (m == "SHADOW") return OperatingMode::Shadow;
    if (m == "PAPER") return OperatingMode::Paper;
    if (m == "REPLAY") return OperatingMode::Replay;
    if (m == "DEMO") return OperatingMode::Shadow;  // operator alias
    return std::nullopt;
}

void BrainRuntime::observe(const BrainSnapshot& snapshot, const BrainFeedRuntime& runtime) {
    latest_ = snapshot;
    runtime_ = runtime;
    has_ = true;
    maybe_publish();
}

void BrainRuntime::maybe_publish() {
    if (control_api_url_.empty() || !has_) return;

    const auto now = std::chrono::steady_clock::now();
    if (last_publish_.time_since_epoch().count() != 0) {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_publish_).count();
        if (elapsed < publish_interval_ms_) return;
    }

    const auto body =
        brain_snapshot_to_json(latest_, model_id_, model_version_, mode_str(mode_), runtime_);
    last_publish_http_ =
        publish_brain_snapshot_to_control_api(body, control_api_url_, pipeline_token_);
    // Liveness for dashboard / client panel — no trading decisions.
    nlohmann::json hb = {{"epics", nlohmann::json::array()}};
    if (!runtime_.market_core_health.empty()) {
        hb["market_core_health"] = runtime_.market_core_health;
    }
    if (!runtime_.execution_health.empty()) {
        hb["execution_health"] = runtime_.execution_health;
    }
    publish_pipeline_heartbeat_to_control_api(hb, control_api_url_, pipeline_token_);
    last_publish_ = now;
    ++publish_attempts_;
}

}  // namespace mr
