#pragma once
#include "mr/capital/capital_client.hpp"
#include "mr/execution_engine/fill_tracker.hpp"
#include "mr/position_brain/position_types.hpp"
#include <string>
#include <unordered_set>
#include <vector>

namespace mr {

struct ReconcileResult {
    std::vector<CapitalPosition> broker_positions;
    std::vector<std::string> missing_at_broker;   // local deal_ids absent at broker
    std::vector<std::string> unknown_at_local;    // broker deal_ids absent locally
    bool ok{false};
};

class Reconciliation {
public:
    Reconciliation(CapitalClient& c, const FillTracker& t) : client_(c), tracker_(t) {}

    /** Legacy boolean: true when broker book is empty OR we have fills (smoke check). */
    bool reconcile();

    /** Stage-10: compare broker open deals vs local open PositionState deal_ids. */
    ReconcileResult reconcile_positions(const std::vector<PositionState>& local_open);

private:
    CapitalClient& client_;
    const FillTracker& tracker_;
};

}  // namespace mr
