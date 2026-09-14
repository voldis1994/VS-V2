#include "mr/market_core/brain_runtime.hpp"
#include "mr/market_core/brain_json.hpp"

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
        case OperatingMode::Live:
        default:
            return "LIVE";
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

void BrainRuntime::observe(const BrainSnapshot& snapshot) {
    latest_ = snapshot;
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
        brain_snapshot_to_json(latest_, model_id_, model_version_, mode_str(mode_));
    last_publish_http_ =
        publish_brain_snapshot_to_control_api(body, control_api_url_, pipeline_token_);
    last_publish_ = now;
    ++publish_attempts_;
}

}  // namespace mr
