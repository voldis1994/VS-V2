#include "mr/market_core/model_bundle_store.hpp"
#include <fstream>
#include <iostream>

namespace mr {

bool ModelBundleStore::load_file(const std::string& path,
                                 MarketCorePipeline& pipeline,
                                 const std::string& model_id,
                                 const std::string& model_version) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "ModelBundleStore: cannot open " << path << std::endl;
        return false;
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        std::cerr << "ModelBundleStore: JSON parse failed: " << e.what() << std::endl;
        return false;
    }
    const auto& src = j.contains("weights") ? j.at("weights") : j;
    WeightBundle bundle = WeightBundle::from_json(src);

    if (current_) {
        previous_ = current_;
        previous_model_id_ = model_id_;
        previous_model_version_ = model_version_;
    }
    current_ = bundle;
    model_id_ = model_id.empty() ? j.value("model_id", model_id_) : model_id;
    model_version_ =
        model_version.empty() ? j.value("model_version", model_version_) : model_version;

    apply_weight_bundle(pipeline, *current_);
    pipeline.brain_runtime().set_model(model_id_, model_version_);
    return true;
}

bool ModelBundleStore::rollback(MarketCorePipeline& pipeline) {
    if (!previous_) return false;
    current_ = previous_;
    model_id_ = previous_model_id_;
    model_version_ = previous_model_version_;
    apply_weight_bundle(pipeline, *current_);
    pipeline.brain_runtime().set_model(model_id_, model_version_);
    previous_.reset();
    return true;
}

}  // namespace mr
