#pragma once

#include "mr/brain/brain_snapshot.hpp"
#include "mr/brain/brain_version.hpp"

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace mr {

/**
 * Runtime telemetry for Control API — never invent ONLINE / 0.0.
 * Missing optionals serialize as null / "UNKNOWN".
 */
struct BrainFeedRuntime {
    std::string market_core_health{"UNKNOWN"};
    std::string feeds_health{"UNKNOWN"};
    std::string execution_health{"UNKNOWN"};
    std::string data_health{"UNKNOWN"};

    std::optional<double> exposure;
    std::optional<double> daily_pnl;
    std::optional<double> max_drawdown;
    std::optional<double> realized_pnl;
};

/** Serialize authoritative BrainSnapshot for Control API (no invented decisions/metrics). */
nlohmann::json brain_snapshot_to_json(const BrainSnapshot& snap,
                                      const std::string& model_id = "default",
                                      const std::string& model_version = "0.0.0",
                                      const std::string& operating_mode = "UNKNOWN",
                                      const BrainFeedRuntime& runtime = {});

/** POST JSON snapshot to Control API pipeline ingest (optional; returns HTTP code or -1). */
int publish_brain_snapshot_to_control_api(const nlohmann::json& body,
                                          const std::string& control_api_url,
                                          const std::string& pipeline_token);

/** POST pipeline heartbeat so Control API can show real market-core liveness. */
int publish_pipeline_heartbeat_to_control_api(const nlohmann::json& body,
                                             const std::string& control_api_url,
                                             const std::string& pipeline_token);

}  // namespace mr
