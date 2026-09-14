#include "mr/market_core/subscription_manager.hpp"

namespace mr {

InstrumentId SubscriptionManager::subscribe_epic(const std::string& epic) {
    std::lock_guard lock(mutex_);
    auto it = by_epic_.find(epic);
    if (it != by_epic_.end()) {
        it->second.active = true;
        return it->second.instrument_id;
    }
    EpicSubscription sub;
    sub.epic = epic;
    sub.instrument_id = next_id_++;
    sub.active = true;
    by_epic_[epic] = sub;
    by_id_[sub.instrument_id] = epic;
    return sub.instrument_id;
}

void SubscriptionManager::unsubscribe_epic(const std::string& epic) {
    std::lock_guard lock(mutex_);
    auto it = by_epic_.find(epic);
    if (it != by_epic_.end()) it->second.active = false;
}

std::vector<EpicSubscription> SubscriptionManager::active() const {
    std::lock_guard lock(mutex_);
    std::vector<EpicSubscription> out;
    for (const auto& [_, sub] : by_epic_) {
        if (sub.active) out.push_back(sub);
    }
    return out;
}

std::string SubscriptionManager::epic_for(InstrumentId id) const {
    std::lock_guard lock(mutex_);
    auto it = by_id_.find(id);
    return it != by_id_.end() ? it->second : "";
}

}  // namespace mr
