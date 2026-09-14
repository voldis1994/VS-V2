#include "mr/market_core/live_capital_bootstrap.hpp"
#include "mr/execution_engine/execution_weight_config.hpp"
#include "mr/execution_engine/retry_policy.hpp"
#include <iostream>

namespace mr {

LiveCapitalBootstrap::LiveCapitalBootstrap(MarketCoreRuntime& runtime, CapitalClient& client)
    : runtime_(runtime), client_(client) {}

bool LiveCapitalBootstrap::prepare(const LiveCapitalBootstrapConfig& cfg) {
    auto& pipe = runtime_.pipeline();
    pipe.set_operating_mode(cfg.operating_mode);
    pipe.brain_runtime().configure_from_env();

    if (cfg.enable_execution) {
        gateway_ = std::make_unique<CapitalOrderGateway>(client_);
        pipe.bind_order_gateway(*gateway_);
        pipe.set_broker_healthy(gateway_->healthy());
        // LIVE retry pacing (tests keep ExecutionWeightConfig::defaults().backoff_ms=0).
        if (pipe.has_execution()) {
            auto w = pipe.execution()->weight_config();
            if (w.backoff_ms == 0) {
                const RetryPolicy retry{};
                w.backoff_ms = retry.backoff_ms;
                w.max_attempts = retry.max_attempts;
                pipe.execution()->set_weight_config(w);
            }
        }
    } else {
        pipe.set_broker_healthy(false);
    }

    if (auto eq = client_.account_equity()) {
        pipe.set_account_equity(*eq);
    } else {
        pipe.clear_account_equity();
        std::cerr << "LiveCapitalBootstrap: account equity unavailable — risk fail-closed\n";
    }

    hydrate_from_broker(cfg.instrument, cfg.epic);
    reconcile_open();

    if (!cfg.model_path.empty()) {
        if (!models_.load_file(cfg.model_path, pipe, cfg.model_id, cfg.model_version)) {
            std::cerr << "LiveCapitalBootstrap: model load failed path=" << cfg.model_path
                      << "\n";
            return false;
        }
    } else {
        pipe.brain_runtime().set_model(cfg.model_id, cfg.model_version);
    }
    return true;
}

void LiveCapitalBootstrap::hydrate_from_broker(InstrumentId instrument,
                                               const std::string& epic) {
    auto& pipe = runtime_.pipeline();
    std::vector<PositionState> recovered;
    for (const auto& bp : client_.positions()) {
        if (!(bp.quantity > 0.0) || bp.deal_id.empty()) continue;
        InstrumentId id = instrument;
        if (!bp.epic.empty()) {
            if (auto mapped = client_.instruments().find_by_epic(bp.epic)) {
                id = *mapped;
            } else if (!epic.empty() && bp.epic != epic) {
                continue;
            }
        }
        PositionState pos;
        pos.instrument = id;
        pos.direction = bp.direction;
        pos.quantity = bp.quantity;
        pos.entry_price = bp.entry_price;
        pos.current_price = bp.entry_price;
        pos.deal_id = bp.deal_id;
        pos.current_pnl = bp.unrealized_pnl;
        recovered.push_back(std::move(pos));
    }
    pipe.hydrate_open_positions(std::move(recovered));
    std::cout << "LiveCapitalBootstrap: hydrated open positions count="
              << pipe.open_positions().size() << std::endl;
}

void LiveCapitalBootstrap::reconcile_open() {
    auto& pipe = runtime_.pipeline();
    if (!pipe.has_execution()) {
        last_reconcile_ = {};
        return;
    }
    Reconciliation recon(client_, pipe.execution()->fills());
    last_reconcile_ = recon.reconcile_positions(pipe.open_positions());
    if (!last_reconcile_.missing_at_broker.empty()) {
        std::cerr << "LiveCapitalBootstrap: reconcile missing_at_broker="
                  << last_reconcile_.missing_at_broker.size() << std::endl;
    }
    if (!last_reconcile_.unknown_at_local.empty()) {
        std::cout << "LiveCapitalBootstrap: broker orphans count="
                  << last_reconcile_.unknown_at_local.size() << std::endl;
    }
}

void LiveCapitalBootstrap::run(LiveFeedConfig feed_cfg, std::atomic<bool>& running) {
    runtime_.run_live(client_, std::move(feed_cfg), running);
}

}  // namespace mr
