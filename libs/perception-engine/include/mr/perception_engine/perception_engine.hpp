#pragma once
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/market_types/market_event.hpp"
namespace mr {
class PerceptionEngineFacade {
public:
    void on_event(const NormalizedEvent& e, double mid);
    [[nodiscard]] PriceDynamics dynamics() const { return engine_.snapshot(); }
private:
    PerceptionEngine engine_;
};
}