#pragma once

#include "mr/brain/brain_snapshot.hpp"
#include "mr/brain/brain_version.hpp"

#include <nlohmann/json.hpp>
#include <string>

namespace mr {

/** Serialize authoritative BrainSnapshot for Control API (no invented decisions). */
nlohmann::json brain_snapshot_to_json(const BrainSnapshot& snap,
                                      const std::string& model_id = "default",
                                      const std::string& model_version = "0.0.0",
                                      const std::string& operating_mode = "LIVE");

/** POST JSON snapshot to Control API pipeline ingest (optional; returns HTTP code or -1). */
int publish_brain_snapshot_to_control_api(const nlohmann::json& body,
                                          const std::string& control_api_url,
                                          const std::string& pipeline_token);

}  // namespace mr
