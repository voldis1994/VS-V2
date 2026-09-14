#include "mr/market_data/market_data_service.hpp"
#include <vector>
namespace mr {
class ReplayProvider : public IMarketDataProvider {
public:
    ReplayProvider(SourceId id, std::vector<MarketEvent> events) : id_(id), events_(std::move(events)) {}
    SourceId source_id() const override { return id_; }
    void start(MarketEventCallback cb) override { running_ = true; cb_ = std::move(cb); for (auto& e : events_) if (cb_) cb_(e); }
    void stop() override { running_ = false; }
    bool is_connected() const override { return running_; }
    HealthStatus health() const override { return running_ ? HealthStatus::Healthy : HealthStatus::Disconnected; }
private:
    SourceId id_; std::vector<MarketEvent> events_; MarketEventCallback cb_; bool running_{false};
};
}