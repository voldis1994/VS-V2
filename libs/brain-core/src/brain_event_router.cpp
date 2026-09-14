#include "mr/brain/brain_event_router.hpp"
namespace mr {
void BrainEventRouter::subscribe(BrainEventType type, BrainEventHandler h) { handlers_[type].push_back(std::move(h)); }
void BrainEventRouter::publish(const BrainEvent& e) {
    auto it = handlers_.find(e.type);
    if (it == handlers_.end()) return;
    for (auto& h : it->second) if (h) h(e);
}
}