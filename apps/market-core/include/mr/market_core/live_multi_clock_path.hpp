#pragma once
#include "mr/market_core/capital_market_source.hpp"
#include "mr/market_core/pipeline.hpp"
#include "mr/common/clock.hpp"
#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace mr {

struct LiveFeedConfig {
    InstrumentId instrument{1};
    SourceId source{1};
    std::string epic{"GOLD"};
    /** Max age for RAW quotes before treated as stale (not structure). */
    double stale_quote_ms{2000.0};
    int quote_poll_ms{250};
    int authority_1m_poll_ms{2000};
    int equity_poll_ms{5000};
    int higher_tf_poll_ms{15000};
    int max_authority_bars{20};
    bool poll_higher_timeframes{true};
};

struct LivePathStats {
    std::uint64_t quotes_processed{0};
    std::uint64_t quotes_stale_dropped{0};
    std::uint64_t quotes_dedup_dropped{0};
    std::uint64_t closed_10s_seen{0};
    std::uint64_t authority_1m_ingested{0};
    std::uint64_t authority_5m_ingested{0};
    std::uint64_t authority_15m_ingested{0};
    std::uint64_t authority_1h_ingested{0};
    std::uint64_t equity_updates{0};
    std::uint64_t equity_missing{0};
    std::uint64_t reconnect_attempts{0};
    std::uint64_t session_failures{0};
};

/**
 * Long-lived multi-clock Capital → market-core data path.
 *
 * RAW quote         → process_event()
 * CLOSED 10s        → one-shot micro (candle engine inside process_event)
 * Capital CLOSED 1m → process_authority_ohlc()
 * 5m/15m/1h         → process_authority_ohlc() (prepared path)
 * Real equity       → RiskEngine via set_account_equity()
 *
 * RAW quote / forming NEVER update structure.
 * Quote-derived 1m is never authority.
 */
class LiveMultiClockPath {
public:
    LiveMultiClockPath(MarketCorePipeline& pipeline, ICapitalMarketSource& source, LiveFeedConfig cfg);

    /** Single poll cycle — used by run() and unit tests. */
    void poll_once(Timestamp now = {});

    /** Blocking loop until running is false. */
    void run(std::atomic<bool>& running);

    [[nodiscard]] const LivePathStats& stats() const { return stats_; }
    [[nodiscard]] const LiveFeedConfig& config() const { return cfg_; }

private:
    void poll_quote(Timestamp now);
    void poll_authority(Timeframe tf, Timestamp now, std::uint64_t& counter);
    void poll_equity();
    bool ensure_connected();

    MarketCorePipeline& pipeline_;
    ICapitalMarketSource& source_;
    LiveFeedConfig cfg_;
    LivePathStats stats_{};
    SteadyTimestamp last_quote_poll_{};
    SteadyTimestamp last_1m_poll_{};
    SteadyTimestamp last_equity_poll_{};
    SteadyTimestamp last_higher_poll_{};
    Timestamp last_quote_ts_{};
    SequenceNumber quote_seq_{0};
    std::unordered_map<std::uint32_t, Timestamp> last_authority_open_;
};

}  // namespace mr
