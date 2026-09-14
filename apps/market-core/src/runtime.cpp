#include "mr/market_core/runtime.hpp"
#include <chrono>
#include <thread>

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

void MarketCoreRuntime::process_authority_ohlc(const Candle& closed, Timeframe tf) {
    if (!running_) return;
    pipeline_.process_authority_ohlc(closed, tf);
}

void MarketCoreRuntime::run_live(CapitalClient& client, LiveFeedConfig feed_cfg,
                                 std::atomic<bool>& running) {
    CapitalClientMarketSource source(client);
    LiveMultiClockPath path(pipeline_, source, std::move(feed_cfg));
    path.run(running);
    running_ = false;
}

void MarketCoreRuntime::run_paper(std::atomic<bool>& running) {
    running_ = true;
    while (running.load() && running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    running_ = false;
}

void MarketCoreRuntime::request_shutdown() {
    running_ = false;
}

RuntimeMode parse_runtime_mode(const std::string& mode) {
    if (mode == "REPLAY") return RuntimeMode::Replay;
    if (mode == "PAPER") return RuntimeMode::Paper;
    if (mode == "DEMO") return RuntimeMode::Demo;
    if (mode == "LIVE") return RuntimeMode::Live;
    if (mode == "SHADOW") return RuntimeMode::Shadow;
    return RuntimeMode::Paper;
}

}  // namespace mr
