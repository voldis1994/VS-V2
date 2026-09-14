#pragma once

#include "mr/market_core/pipeline.hpp"
#include "mr/market_core/live_multi_clock_path.hpp"
#include "mr/market_core/capital_client_source.hpp"
#include "mr/capital/capital_client.hpp"
#include "mr/common/config.hpp"
#include <atomic>
#include <memory>
#include <string>

namespace mr {

enum class RuntimeMode { Replay, Paper, Demo, Live };

class MarketCoreRuntime {
public:
    MarketCoreRuntime();
    void configure(const ConfigRegistry& config, RuntimeMode mode);
    void process_event(const MarketEvent& event);
    void process_authority_ohlc(const Candle& closed, Timeframe tf);

    /** Attach Capital client + epic mapping and run the long-lived multi-clock path. */
    void run_live(CapitalClient& client, LiveFeedConfig feed_cfg, std::atomic<bool>& running);

    /**
     * Persistent PAPER/REPLAY idle loop — Stage 2 requires the process to stay alive
     * until `running` is cleared (SIGINT/SIGTERM) or request_shutdown().
     */
    void run_paper(std::atomic<bool>& running);

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
