#include "mr/perception_engine/price_dynamics.hpp"
#include <algorithm>
namespace mr {
void PerceptionEngine::update(double price, Timestamp ts) {
    samples_.push({ts, price});
    if (prev_price_ > 0) {
        current_.return_value = price - prev_price_;
        current_.normalized_return = current_.return_value / prev_price_;
        current_.velocity = current_.return_value;
        current_.acceleration = current_.velocity - prev_velocity_;
        prev_velocity_ = current_.velocity;
        if (current_.return_value > 0) current_.directional_persistence = current_.directional_persistence * 0.9 + 1.0;
        else if (current_.return_value < 0) current_.directional_persistence = current_.directional_persistence * 0.9 - 1.0;
        else current_.directional_persistence *= 0.9;
    }
    prev_price_ = price;
    if (session_high_ == 0 || price > session_high_) session_high_ = price;
    if (session_low_ == 0 || price < session_low_) session_low_ = price;
    if (session_high_ > session_low_) current_.displacement = (price - session_low_) / (session_high_ - session_low_);
    if (samples_.size() >= 2) {
        double oldest = samples_.at(samples_.size() - 1).price;
        double newest = samples_.newest().price;
        current_.short_horizon_momentum = newest - oldest;
    }
}
void PerceptionEngine::reset() { samples_.clear(); current_ = {}; prev_price_ = prev_velocity_ = session_high_ = session_low_ = 0; }
}