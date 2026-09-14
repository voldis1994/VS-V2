#pragma once
#include "mr/decision_engine/decision_types.hpp"
namespace mr {
enum class ExitReason : std::uint8_t { None=0, ThesisFailure=1, HardInvalidation=2, PeakProtection=3, Target=4, TimeDecay=5, ReversalEvidence=6, EmergencyStop=7 };
enum class PositionAction : std::uint8_t { Hold=0, ExitNow=1, Trail=2, TakeProfit=3, Reduce=4, Protect=5 };
struct PositionState {
    PositionId id{0}; TradeIntentId intent_id{0}; InstrumentId instrument{kInvalidInstrument};
    Direction direction{Direction::Flat}; double entry_price{0}, quantity{0}, current_price{0};
    Timestamp opened_at{}; double mfe{0}, mae{0}, peak_favorable_price{0}, peak_retention{0};
    double current_pnl{0}, stop_loss{0}, take_profit{0}; std::uint64_t horizon_ns{10'000'000'000};
};
struct PositionDecision {
    PositionAction action{PositionAction::Hold}; ExitReason reason{ExitReason::None};
    double ev_exit{0}, ev_hold{0}, continuation_probability{0}, reversal_probability{0};
    std::vector<std::string> reason_codes;
};
}