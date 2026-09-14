#pragma once
#include "mr/decision/trade_decision.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace mr {

enum class ExitReason : std::uint8_t {
    None = 0,
    ThesisFailure = 1,
    HardInvalidation = 2,
    PeakProtection = 3,
    Target = 4,
    TimeDecay = 5,
    ReversalEvidence = 6,
    EmergencyStop = 7
};

/** Continuous management actions after entry. */
enum class PositionAction : std::uint8_t {
    Hold = 0,
    Protect = 1,
    Reduce = 2,
    Exit = 3
};

struct PositionState {
    PositionId id{0};
    TradeIntentId intent_id{0};
    InstrumentId instrument{kInvalidInstrument};
    Direction direction{Direction::Flat};
    double entry_price{0};
    double quantity{0};
    double current_price{0};
    Timestamp opened_at{};
    double mfe{0};
    double mae{0};
    double peak_favorable_price{0};
    double peak_retention{0};
    double current_pnl{0};
    double stop_loss{0};
    double take_profit{0};
    std::uint64_t horizon_ns{10'000'000'000ULL};

    // Entry thesis snapshot
    double entry_thesis_quality{0};
    double entry_continuation{0};
    double entry_invalidation{0};
    std::string deal_id;
};

struct PositionDecision {
    PositionAction action{PositionAction::Hold};
    ExitReason reason{ExitReason::None};
    double ev_exit{0};
    double ev_hold{0};
    double continuation_strength{0};
    double degradation{0};
    double reduce_fraction{0};
    double suggested_stop{0};
    double suggested_target{0};
    std::vector<std::string> reason_codes;
};

}  // namespace mr
