#include "mr/market_core/event_dispatcher.hpp"

namespace mr {

void EventDispatcher::subscribe(MarketEventHandler handler) {
    std::lock_guard lock(mutex_);
    handlers_.push_back(std::move(handler));
}

void EventDispatcher::dispatch(const MarketEvent& event) {
    std::lock_guard lock(mutex_);
    for (const auto& handler : handlers_) {
        handler(event);
    }
}

}  // namespace mr
