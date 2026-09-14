#pragma once
#include <cstdint>
namespace mr {
enum class BrainEventType : std::uint8_t {
    Quote=0, CandleClosed=1, Perception=2, Structure=3, Scenario=4, Decision=5, Position=6
};
}