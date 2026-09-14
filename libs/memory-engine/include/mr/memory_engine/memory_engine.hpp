#pragma once
#include "mr/memory_engine/memory_types.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
namespace mr {
class MemoryEngine {
public:
    void update(double price, const PriceDynamics& pd, bool trade_closed, double pnl);
    [[nodiscard]] const MemoryBank& bank() const { return bank_; }
private:
    MemoryBank bank_;
};
}