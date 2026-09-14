#pragma once
#include "mr/market_types/candle.hpp"
#include "mr/market_types/timeframe.hpp"
#include "mr/common/id.hpp"
#include <cstdint>
#include <optional>

namespace mr {

/** Strict clock domains — never collapse these into one event. */
enum class MarketClockKind : std::uint8_t {
    RawQuote = 0,
    FormingCandle = 1,
    ClosedTenSecond = 2,
    ClosedOneMinute = 3,
    ClosedHigherTimeframe = 4
};

struct MarketClockEvent {
    MarketClockKind kind{MarketClockKind::RawQuote};
    InstrumentId instrument{kInvalidInstrument};
    Timestamp ts{};
    Timeframe timeframe{Timeframe::Second1};
    std::optional<Candle> candle{};
    /** True only for the first emission of a specific closed bar open_time. */
    bool one_shot{false};
    /**
     * Structure/context may update only when this is true.
     * Set only for Capital (broker) closed 1m+ OHLC authority bars.
     */
    bool structure_authority{false};
};

inline bool is_structure_authority(const MarketClockEvent& ev) {
    return ev.structure_authority
        && (ev.kind == MarketClockKind::ClosedOneMinute
            || ev.kind == MarketClockKind::ClosedHigherTimeframe);
}

}  // namespace mr
