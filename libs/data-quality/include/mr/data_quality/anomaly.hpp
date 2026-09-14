#pragma once
#include "mr/market_types/market_event.hpp"
namespace mr {
struct AnomalyResult { bool crossed{false}; bool wide_spread{false}; bool price_jump{false}; };
class AnomalyDetector {
public:
    void configure(double max_spread, double max_jump_pct);
    AnomalyResult check(const NormalizedEvent& e, double prev_mid);
private:
    double max_spread_{10.0}, max_jump_pct_{0.05};
};
}