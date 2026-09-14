#pragma once
#include "mr/brain_core/brain_snapshot.hpp"
namespace mr {
class BrainState {
public:
    void update(const BrainContext& ctx);
    [[nodiscard]] const BrainSnapshot& latest() const { return snapshot_; }
private:
    BrainSnapshot snapshot_;
};
}