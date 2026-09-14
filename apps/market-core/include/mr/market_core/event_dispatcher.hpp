#pragma once

#include "mr/market_types/market_event.hpp"
#include <functional>
#include <mutex>
#include <vector>

namespace mr {

using MarketEventHandler = std::function<void(const MarketEvent&)>;

class EventDispatcher {
public:
    void subscribe(MarketEventHandler handler);
    void dispatch(const MarketEvent& event);

private:
    std::mutex mutex_;
    std::vector<MarketEventHandler> handlers_;
};

}  // namespace mr
