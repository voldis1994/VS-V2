#pragma once
#include "mr/microstructure_engine/microstructure_features.hpp"
#include "mr/market_types/market_event.hpp"
namespace mr {
class MicrostructureEngine {
public:
    void update(const NormalizedEvent& e);
    [[nodiscard]] MicrostructureFeatures snapshot() const { return f_; }
private:
    MicrostructureFeatures f_;
    std::uint64_t trade_count_{0}, quote_count_{0};
};
}