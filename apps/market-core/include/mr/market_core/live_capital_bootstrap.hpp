#pragma once

#include "mr/capital/capital_client.hpp"
#include "mr/execution_engine/order_gateway.hpp"
#include "mr/execution_engine/reconciliation.hpp"
#include "mr/market_core/model_bundle_store.hpp"
#include "mr/market_core/pipeline.hpp"
#include "mr/market_core/runtime.hpp"
#include <atomic>
#include <memory>
#include <string>

namespace mr {

struct LiveCapitalBootstrapConfig {
    InstrumentId instrument{1};
    std::string epic;
    std::string model_path;
    std::string model_id{"default"};
    std::string model_version{"0.0.0"};
    bool enable_execution{true};
    OperatingMode operating_mode{OperatingMode::Live};
};

/**
 * Capital bootstrap for LIVE (execution) and PAPER (market data only).
 * PAPER/REPLAY: never bind CapitalOrderGateway even if enable_execution is set.
 * LIVE/SHADOW: may bind execution when enable_execution is true.
 * Never invents BUY/SELL — only connects the existing Stage 1–9 chain.
 */
class LiveCapitalBootstrap {
public:
    LiveCapitalBootstrap(MarketCoreRuntime& runtime, CapitalClient& client);

    bool prepare(const LiveCapitalBootstrapConfig& cfg);
    void run(LiveFeedConfig feed_cfg, std::atomic<bool>& running);

    [[nodiscard]] ModelBundleStore& models() { return models_; }
    [[nodiscard]] CapitalOrderGateway* gateway() { return gateway_.get(); }
    [[nodiscard]] const ReconcileResult& last_reconcile() const { return last_reconcile_; }

private:
    void hydrate_from_broker(InstrumentId instrument, const std::string& epic);
    void reconcile_open();

    MarketCoreRuntime& runtime_;
    CapitalClient& client_;
    std::unique_ptr<CapitalOrderGateway> gateway_;
    ModelBundleStore models_;
    ReconcileResult last_reconcile_{};
};

}  // namespace mr
