#include "mr/market_data/subscription.hpp"
#include "mr/market_types/quote.hpp"
#include <algorithm>
#include <vector>
namespace mr {
void SubscriptionManager::subscribe(const Subscription& sub) { subs_.push_back(sub); }
void SubscriptionManager::unsubscribe(InstrumentId instrument, SourceId source) {
    subs_.erase(std::remove_if(subs_.begin(), subs_.end(), [&](const Subscription& s) {
        return s.instrument == instrument && s.source == source;
    }), subs_.end());
}
void SubscriptionManager::dispatch(const Quote& q) {
    for (const auto& s : subs_) if (s.instrument == q.instrument && s.handler) s.handler(q);
}
}