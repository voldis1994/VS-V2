#pragma once
#include "mr/capital/capital_quote.hpp"
#include "mr/capital/capital_prices.hpp"
#include "mr/market_types/timeframe.hpp"
#include "mr/common/id.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace mr {

/**
 * Abstract Capital market source — real client or test double.
 * Keeps TypeScript out of the trading data path.
 */
class ICapitalMarketSource {
public:
    virtual ~ICapitalMarketSource() = default;
    virtual bool ensure_session() = 0;
    virtual void invalidate_session() = 0;
    [[nodiscard]] virtual std::optional<CapitalQuote> fetch_quote(InstrumentId instrument) = 0;
    [[nodiscard]] virtual CapitalPriceHistory fetch_closed_ohlc(
        const std::string& epic, Timeframe tf, int max_bars) = 0;
    [[nodiscard]] virtual std::optional<double> fetch_equity() = 0;
    [[nodiscard]] virtual std::uint64_t reconnect_count() const = 0;
};

}  // namespace mr
