#pragma once
#include "mr/common/id.hpp"
#include <string>
#include <vector>
namespace mr {
enum class TradeAction : std::uint8_t { Wait=0, Buy=1, Sell=2 };
enum class EntryDecision : std::uint8_t { NoTrade=0, EntryReady=1, Reject=2 };
struct Opportunity {
    InstrumentId instrument{kInvalidInstrument};
    Direction direction{Direction::Flat};
    double probability{0}, expected_value{0}, spread_cost{0};
    TradeAction action{TradeAction::Wait};
    std::vector<std::string> reason_codes;
};
struct TradeIntent {
    TradeIntentId id{0}; InstrumentId instrument{kInvalidInstrument};
    Direction direction{Direction::Flat}; Timestamp created_at{}, expires_at{};
    double reference_price{0}, probability{0}, expected_value{0}, stop_loss{0}, take_profit{0};
    EntryDecision decision{EntryDecision::NoTrade};
    std::vector<std::string> reason_codes;
    std::string explanation;
};
}