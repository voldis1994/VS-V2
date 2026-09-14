#pragma once
#include "mr/market_data/quote_stream.hpp"
#include "mr/market_data/subscription.hpp"
#include "mr/market_types/market_event.hpp"
#include <functional>
#include <memory>
#include <vector>
namespace mr {
using MarketEventCallback = std::function<void(const MarketEvent&)>;
class IMarketDataProvider {
public:
    virtual ~IMarketDataProvider() = default;
    virtual SourceId source_id() const = 0;
    virtual void start(MarketEventCallback cb) = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool is_connected() const = 0;
    [[nodiscard]] virtual HealthStatus health() const = 0;
};
class MarketDataService {
public:
    void add_provider(std::shared_ptr<IMarketDataProvider> p);
    void start();
    void stop();
    QuoteStream& quotes() { return stream_; }
private:
    std::vector<std::shared_ptr<IMarketDataProvider>> providers_;
    QuoteStream stream_;
    SubscriptionManager subs_;
};
}