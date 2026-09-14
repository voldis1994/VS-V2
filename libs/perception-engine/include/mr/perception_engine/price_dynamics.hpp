#pragma once
#include "mr/common/rolling_window.hpp"
#include "mr/common/id.hpp"
namespace mr {
struct PriceDynamics {
    double return_value{0}, normalized_return{0}, velocity{0}, acceleration{0};
    double directional_persistence{0}, short_horizon_momentum{0}, displacement{0};
};
struct WindowSample { Timestamp ts{}; double price{0}; };
class PerceptionEngine {
public:
    void update(double price, Timestamp ts);
    [[nodiscard]] PriceDynamics snapshot() const { return current_; }
    void reset();
private:
    RollingWindow<WindowSample, 4096> samples_;
    PriceDynamics current_;
    double prev_price_{0}, prev_velocity_{0}, session_high_{0}, session_low_{0};
};
}