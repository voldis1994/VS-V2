#include "mr/candle_engine/candle_validator.hpp"
namespace mr {
ValidationResult CandleValidator::validate(const Candle& c) const {
    ValidationResult r;
    if (c.high < c.low) { r.ok = false; r.reason = "high < low"; return r; }
    if (c.open > c.high || c.open < c.low || c.close > c.high || c.close < c.low) {
        r.ok = false; r.reason = "ohlc out of range"; return r;
    }
    if (c.ticks == 0 && c.status != CandleStatus::Gap) { r.ok = false; r.reason = "no ticks"; }
    return r;
}
}