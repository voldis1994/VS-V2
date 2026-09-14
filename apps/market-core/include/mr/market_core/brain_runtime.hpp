#pragma once

#include "mr/brain/market_brain.hpp"
#include "mr/common/id.hpp"
#include "mr/market_core/brain_json.hpp"
#include <chrono>
#include <cstdint>
#include <string>

namespace mr {

/**
 * Tracks BrainSnapshot and optionally publishes authoritative JSON to Control API.
 * Does not invent decisions — relays BrainSnapshot only.
 */
class BrainRuntime {
public:
    void set_control_api_url(std::string url) { control_api_url_ = std::move(url); }
    void set_pipeline_token(std::string token) { pipeline_token_ = std::move(token); }
    void set_operating_mode(OperatingMode mode) { mode_ = mode; }
    void set_model(std::string model_id, std::string model_version) {
        model_id_ = std::move(model_id);
        model_version_ = std::move(model_version);
    }
    void set_publish_interval_ms(std::int64_t ms) { publish_interval_ms_ = ms; }

    /** Load CONTROL_API_URL / PIPELINE_TOKEN / PIPELINE_SERVICE_TOKEN from env. */
    void configure_from_env();

    void observe(const BrainSnapshot& snapshot, const BrainFeedRuntime& runtime = {});

    [[nodiscard]] BrainSnapshot latest() const { return latest_; }
    [[nodiscard]] BrainFeedRuntime latest_runtime() const { return runtime_; }
    [[nodiscard]] bool has_snapshot() const { return has_; }
    [[nodiscard]] std::uint64_t publish_attempts() const { return publish_attempts_; }
    [[nodiscard]] int last_publish_http() const { return last_publish_http_; }

private:
    void maybe_publish();

    BrainSnapshot latest_{};
    BrainFeedRuntime runtime_{};
    bool has_{false};
    std::string control_api_url_;
    std::string pipeline_token_;
    std::string model_id_{"default"};
    std::string model_version_{"0.0.0"};
    OperatingMode mode_{OperatingMode::Live};
    std::int64_t publish_interval_ms_{500};
    std::chrono::steady_clock::time_point last_publish_{};
    std::uint64_t publish_attempts_{0};
    int last_publish_http_{-1};
};

inline void sync_brain_runtime(BrainRuntime& runtime, MarketBrain& brain) {
    runtime.observe(brain.snapshot());
}

}  // namespace mr
