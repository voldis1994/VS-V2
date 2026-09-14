#pragma once
#include "mr/decision/opportunity.hpp"
#include "mr/common/id.hpp"
#include <string>
#include <vector>
namespace mr {
enum class EntryDecision : std::uint8_t { NoTrade = 0, EntryReady = 1, Reject = 2 };
struct TradeIntent {
    TradeIntentId id{0};
    InstrumentId instrument{kInvalidInstrument};
    Direction direction{Direction::Flat};
    Timestamp created_at{};
    Timestamp expires_at{};
    double reference_price{0};
    double probability{0};
    double expected_value{0};
    double stop_loss{0};
    double take_profit{0};
    EntryDecision decision{EntryDecision::NoTrade};
    std::vector<std::string> reason_codes;
    std::string explanation;
};
using TradeDecision = TradeIntent;
}
