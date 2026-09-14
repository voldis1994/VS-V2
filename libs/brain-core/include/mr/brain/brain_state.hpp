#pragma once
#include "mr/brain/brain_snapshot.hpp"
#include "mr/brain/brain_context.hpp"
namespace mr {
class BrainState {
public:
    void update(const BrainContext& ctx);
    [[nodiscard]] const BrainSnapshot& latest() const { return snapshot_; }
private:
    BrainSnapshot snapshot_;
};
}
