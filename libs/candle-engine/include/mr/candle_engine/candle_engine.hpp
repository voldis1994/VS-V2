#pragma once
#include "mr/candle_engine/ten_second_builder.hpp"
#include "mr/candle_engine/second_builder.hpp"
#include "mr/candle_engine/candle_history.hpp"
#include "mr/candle_engine/candle_deduplicator.hpp"
#include "mr/candle_engine/candle_gap_handler.hpp"
#include "mr/candle_engine/candle_validator.hpp"
#include "mr/market_types/market_event.hpp"
namespace mr {
struct CandleEngineState {
    Candle forming_10s{};
    ClosedCandle last_closed_10s{};
    bool has_forming{false};
    bool has_closed{false};
    double bucket_progress{0};
};
class CandleEngine {
public:
    void on_quote(double price, Timestamp ts, InstrumentId inst);
    void on_event(const NormalizedEvent& e, double consensus_mid);
    [[nodiscard]] CandleEngineState state() const;
    [[nodiscard]] const CandleHistory& history() const { return history_; }
    void reset();
private:
    TenSecondBuilder builder_10s_;
    SecondBuilder builder_1s_;
    CandleHistory history_;
    CandleDeduplicator dedup_;
    CandleGapHandler gaps_;
    CandleValidator validator_;
    CandleEngineState state_{};
    InstrumentId instrument_{kInvalidInstrument};
};
}