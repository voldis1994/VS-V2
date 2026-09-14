#pragma once

#include "mr/market_core/pipeline.hpp"
#include "mr/market_core/weight_bundle.hpp"
#include <optional>
#include <string>

namespace mr {

/**
 * Production model/weight loader with one-step rollback.
 * Does not invent trading decisions — only applies calibrated weight bundles.
 */
class ModelBundleStore {
public:
    bool load_file(const std::string& path,
                   MarketCorePipeline& pipeline,
                   const std::string& model_id,
                   const std::string& model_version);

    bool rollback(MarketCorePipeline& pipeline);

    [[nodiscard]] const std::string& model_id() const { return model_id_; }
    [[nodiscard]] const std::string& model_version() const { return model_version_; }
    [[nodiscard]] bool has_current() const { return current_.has_value(); }
    [[nodiscard]] bool has_previous() const { return previous_.has_value(); }

private:
    std::optional<WeightBundle> current_;
    std::optional<WeightBundle> previous_;
    std::string model_id_{"default"};
    std::string model_version_{"0.0.0"};
    std::string previous_model_id_;
    std::string previous_model_version_;
};

}  // namespace mr
