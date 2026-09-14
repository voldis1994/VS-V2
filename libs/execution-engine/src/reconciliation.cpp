#include "mr/execution_engine/reconciliation.hpp"
#include <unordered_set>

namespace mr {

bool Reconciliation::reconcile() {
    auto positions = client_.positions();
    return !positions.empty() || tracker_.fills().empty();
}

ReconcileResult Reconciliation::reconcile_positions(const std::vector<PositionState>& local_open) {
    ReconcileResult out;
    out.broker_positions = client_.positions();

    std::unordered_set<std::string> broker_deals;
    for (const auto& bp : out.broker_positions) {
        if (!bp.deal_id.empty()) broker_deals.insert(bp.deal_id);
    }

    std::unordered_set<std::string> local_deals;
    for (const auto& lp : local_open) {
        if (lp.deal_id.empty()) continue;
        local_deals.insert(lp.deal_id);
        if (!broker_deals.count(lp.deal_id)) {
            out.missing_at_broker.push_back(lp.deal_id);
        }
    }

    for (const auto& bp : out.broker_positions) {
        if (bp.deal_id.empty()) continue;
        if (!local_deals.count(bp.deal_id)) {
            out.unknown_at_local.push_back(bp.deal_id);
        }
    }

    out.ok = out.missing_at_broker.empty();  // orphans at broker are recoverable via hydrate
    return out;
}

}  // namespace mr
