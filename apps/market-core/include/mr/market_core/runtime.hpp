#pragma once

#include "mr/market_core/pipeline.hpp"
#include "mr/common/config.hpp"
#include <atomic>
#include <functional>
#include <string>

namespace mr {

enum class RuntimeMode { Replay, Paper, Demo, Live };

class MarketCoreRuntime {
public:
    MarketCoreRuntime();
    void configure(const ConfigRegistry& config, RuntimeMode mode);
    void process_event(const MarketEvent& event);
    [[nodiscard]] MarketCorePipeline& pipeline() { return pipeline_; }
    [[nodiscard]] RuntimeMode mode() const { return mode_; }
    void request_shutdown();
    [[nodiscard]] bool running() const { return running_; }

private:
    MarketCorePipeline pipeline_;
    RuntimeMode mode_{RuntimeMode::Paper};
    std::atomic<bool> running_{true};
};

RuntimeMode parse_runtime_mode(const std::string& mode);

}  // namespace mr
