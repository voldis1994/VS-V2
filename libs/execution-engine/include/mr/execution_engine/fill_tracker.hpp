#pragma once
#include "mr/capital/capital_order.hpp"
#include <vector>
namespace mr {
struct FillRecord { CapitalOrderResponse response; Timestamp ts{}; };
class FillTracker {
public:
    void record(const FillRecord& f);
    [[nodiscard]] const std::vector<FillRecord>& fills() const { return fills_; }
private:
    std::vector<FillRecord> fills_;
};
}