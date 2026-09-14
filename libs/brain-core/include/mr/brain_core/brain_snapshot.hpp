#pragma once
#include "mr/brain_core/brain_context.hpp"
#include <unordered_map>
namespace mr {
struct BrainSnapshot {
    SnapshotId id{0};
    Timestamp ts{};
    std::unordered_map<InstrumentId, BrainContext> instruments;
};
}