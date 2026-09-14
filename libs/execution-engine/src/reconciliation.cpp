#include "mr/execution_engine/reconciliation.hpp"
namespace mr {
bool Reconciliation::reconcile() {
    auto positions = client_.positions();
    return !positions.empty() || tracker_.fills().empty();
}
}