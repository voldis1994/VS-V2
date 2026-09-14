#pragma once

#include "mr/common/types.hpp"
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mr {

struct EpicSubscription {
    std::string epic;
    InstrumentId instrument_id{kInvalidInstrument};
    bool active{true};
};

class SubscriptionManager {
public:
    InstrumentId subscribe_epic(const std::string& epic);
    void unsubscribe_epic(const std::string& epic);
    [[nodiscard]] std::vector<EpicSubscription> active() const;
    [[nodiscard]] std::string epic_for(InstrumentId id) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, EpicSubscription> by_epic_;
    std::unordered_map<InstrumentId, std::string> by_id_;
    InstrumentId next_id_{1};
};

}  // namespace mr
