#pragma once
#include "mr/candle_engine/candle_event.hpp"
#include "mr/market_types/candle.hpp"
#include "mr/market_types/timeframe.hpp"
#include <cstdint>
namespace mr {
class CandleBuilder {
public:
    explicit CandleBuilder(Timeframe tf) : tf_(tf), bucket_ns_(timeframe_ns(tf)) {}
    virtual ~CandleBuilder() = default;
    virtual CandleEvent on_tick(double price, Timestamp ts, InstrumentId inst);
    [[nodiscard]] const Candle& forming() const { return forming_; }
    [[nodiscard]] bool has_forming() const { return has_forming_; }
protected:
    virtual std::uint64_t bucket_start(Timestamp ts) const;
    Timeframe tf_;
    std::uint64_t bucket_ns_;
    Candle forming_{};
    bool has_forming_{false};
};
}