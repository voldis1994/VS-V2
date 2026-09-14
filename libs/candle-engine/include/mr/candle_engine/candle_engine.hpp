#pragma once
#include "mr/candle_engine/closed_candle.hpp"
#include "mr/candle_engine/ten_second_builder.hpp"
#include "mr/candle_engine/second_builder.hpp"
#include "mr/candle_engine/timeframe_aggregator.hpp"
#include "mr/candle_engine/candle_history.hpp"
#include "mr/candle_engine/candle_deduplicator.hpp"
#include "mr/candle_engine/candle_gap_handler.hpp"
#include "mr/candle_engine/candle_validator.hpp"
#include "mr/market_types/market_event.hpp"
#include "mr/market_types/market_clock.hpp"
#include <vector>

namespace mr {

struct CandleEngineState {
    Candle forming_10s{};
    ClosedCandle last_closed_10s{};
    ClosedCandle last_closed_1m{};
    ClosedCandle last_authority{};  // Capital closed 1m+ OHLC
    bool has_forming{false};
    bool has_closed_10s{false};
    bool has_closed{false};  // alias of has_closed_10s for consumers/tests
    bool has_closed_1m{false};
    bool has_authority{false};
    double bucket_progress{0};
};

/**
 * RAW QUOTE drives forming + may emit one-shot CLOSED_10s (and derived CLOSED_1m).
 * Structure authority is ONLY ingest_authority_ohlc (Capital closed 1m+).
 */
class CandleEngine {
public:
    std::vector<MarketClockEvent> on_quote(double price, Timestamp ts, InstrumentId inst);
    std::vector<MarketClockEvent> on_event(const NormalizedEvent& e, double consensus_mid);

    /** Capital/broker closed OHLC for Minute1+. Marks structure_authority=true. */
    std::vector<MarketClockEvent> ingest_authority_ohlc(const Candle& closed, Timeframe tf);

    [[nodiscard]] CandleEngineState state() const;
    [[nodiscard]] const CandleHistory& history() const { return history_; }
    void reset();

private:
    TenSecondBuilder builder_10s_;
    SecondBuilder builder_1s_;
    TimeframeAggregator agg_1m_{Timeframe::Minute1};
    CandleHistory history_;
    CandleDeduplicator dedup_10s_;
    CandleDeduplicator dedup_1m_;
    CandleDeduplicator dedup_authority_;
    CandleGapHandler gaps_;
    CandleValidator validator_;
    CandleEngineState state_{};
    InstrumentId instrument_{kInvalidInstrument};
    Timestamp last_emitted_10s_open_{};
    bool has_emitted_10s_{false};
};

}  // namespace mr
