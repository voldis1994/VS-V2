#pragma once

#include "mr/risk/risk_engine.hpp"
#include <functional>
#include <string>
#include <vector>

namespace mr {

struct IntentPublishResult {
    bool ok{false};
    std::string error;
};

using IntentPublisher = std::function<IntentPublishResult(const RiskIntent&, const std::string& epic)>;

class IntentDispatcher {
public:
    void set_publisher(IntentPublisher publisher);
    std::vector<IntentPublishResult> publish_batch(const std::vector<RiskIntent>& intents,
                                                   const std::function<std::string(InstrumentId)>& epic_lookup);

private:
    IntentPublisher publisher_;
};

}  // namespace mr
