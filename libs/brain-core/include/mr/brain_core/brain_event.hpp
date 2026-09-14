#pragma once
#include "mr/brain_core/brain_event_type.hpp"
#include "mr/common/id.hpp"
#include <string>
namespace mr {
struct BrainEvent {
    BrainEventType type{BrainEventType::Quote};
    Timestamp ts{};
    InstrumentId instrument{kInvalidInstrument};
    std::string payload;
};
}