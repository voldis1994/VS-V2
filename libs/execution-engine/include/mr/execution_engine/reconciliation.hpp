#pragma once
#include "mr/capital/capital_client.hpp"
#include "mr/execution_engine/fill_tracker.hpp"
namespace mr {
class Reconciliation {
public:
    Reconciliation(CapitalClient& c, FillTracker& t) : client_(c), tracker_(t) {}
    bool reconcile();
private:
    CapitalClient& client_; FillTracker& tracker_;
};
}