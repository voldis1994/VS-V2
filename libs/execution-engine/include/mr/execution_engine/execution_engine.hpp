#pragma once
#include "mr/execution_engine/capital_executor.hpp"
#include "mr/execution_engine/fill_tracker.hpp"
#include "mr/execution_engine/retry_policy.hpp"
#include "mr/execution_engine/reconciliation.hpp"
#include "mr/risk_engine/risk_engine.hpp"
namespace mr {
class ExecutionEngine {
public:
    ExecutionEngine(CapitalClient& client, RiskEngine& risk);
    CapitalOrderResponse submit(const TradeIntent& intent, double quantity);
private:
    CapitalClient& client_; RiskEngine& risk_;
    OrderBuilder builder_; CapitalExecutor executor_; FillTracker fills_;
    RetryPolicy retry_; Reconciliation recon_;
};
}