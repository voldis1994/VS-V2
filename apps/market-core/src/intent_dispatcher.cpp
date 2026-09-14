#include "mr/market_core/intent_dispatcher.hpp"

namespace mr {

void IntentDispatcher::set_publisher(IntentPublisher publisher) {
    publisher_ = std::move(publisher);
}

std::vector<IntentPublishResult> IntentDispatcher::publish_batch(
    const std::vector<RiskIntent>& intents,
    const std::function<std::string(InstrumentId)>& epic_lookup) {
    std::vector<IntentPublishResult> results;
    if (!publisher_) {
        IntentPublishResult r;
        r.ok = false;
        r.error = "No publisher configured";
        results.push_back(r);
        return results;
    }
    for (const auto& intent : intents) {
        const auto epic = epic_lookup(intent.instrument);
        results.push_back(publisher_(intent, epic));
    }
    return results;
}

}  // namespace mr
