#include "mr/market_core/runtime.hpp"

namespace mr {

MarketCoreRuntime::MarketCoreRuntime() = default;

void MarketCoreRuntime::configure(const ConfigRegistry& config, RuntimeMode mode) {
    mode_ = mode;
    pipeline_.configure(config);
}

void MarketCoreRuntime::process_event(const MarketEvent& event) {
    if (!running_) return;
    pipeline_.process_event(event);
}

void MarketCoreRuntime::request_shutdown() {
    running_ = false;
}

RuntimeMode parse_runtime_mode(const std::string& mode) {
    if (mode == "REPLAY") return RuntimeMode::Replay;
    if (mode == "PAPER") return RuntimeMode::Paper;
    if (mode == "DEMO") return RuntimeMode::Demo;
    if (mode == "LIVE") return RuntimeMode::Live;
    return RuntimeMode::Paper;
}

}  // namespace mr
