#pragma once
#include "mr/brain/brain_event.hpp"
#include <functional>
#include <unordered_map>
#include <vector>
namespace mr {
using BrainEventHandler = std::function<void(const BrainEvent&)>;
class BrainEventRouter {
public:
    void subscribe(BrainEventType type, BrainEventHandler h);
    void publish(const BrainEvent& e);
private:
    std::unordered_map<BrainEventType, std::vector<BrainEventHandler>> handlers_;
};
}
