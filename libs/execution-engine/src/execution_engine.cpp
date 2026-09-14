#include "mr/execution_engine/execution_engine.hpp"
#include "mr/common/clock.hpp"
namespace mr {
ExecutionEngine::ExecutionEngine(CapitalClient& client, RiskEngine& risk)
    : client_(client), risk_(risk), executor_(client), recon_(client, fills_) {}
CapitalOrderResponse ExecutionEngine::submit(const TradeIntent& intent, double quantity) {
    auto guard = risk_.pre_trade_check(intent, 0);
    if (!guard.pass) { CapitalOrderResponse r; r.error_message = guard.reason; return r; }
    auto req = builder_.from_intent(intent, quantity);
    CapitalOrderResponse resp;
    for (std::uint32_t i = 0; i < retry_.max_attempts; ++i) {
        resp = executor_.execute(req);
        if (resp.success) { fills_.record({resp, now_utc_ns()}); recon_.reconcile(); return resp; }
    }
    return resp;
}
}