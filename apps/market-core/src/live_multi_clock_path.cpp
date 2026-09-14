#include "mr/market_core/live_multi_clock_path.hpp"
#include "mr/capital/capital_parse.hpp"
#include "mr/market_types/market_event.hpp"
#include <chrono>
#include <thread>

namespace mr {

LiveMultiClockPath::LiveMultiClockPath(
    MarketCorePipeline& pipeline, ICapitalMarketSource& source, LiveFeedConfig cfg)
    : pipeline_(pipeline), source_(source), cfg_(std::move(cfg)) {}

bool LiveMultiClockPath::ensure_connected() {
    if (source_.ensure_session()) {
        pipeline_.set_broker_healthy(true);
        return true;
    }
    ++stats_.session_failures;
    ++stats_.reconnect_attempts;
    pipeline_.set_broker_healthy(false);
    source_.invalidate_session();
    if (source_.ensure_session()) {
        ++stats_.reconnect_attempts;
        pipeline_.set_broker_healthy(true);
        return true;
    }
    ++stats_.session_failures;
    pipeline_.set_broker_healthy(false);
    return false;
}

void LiveMultiClockPath::poll_quote(Timestamp now) {
    auto q = source_.fetch_quote(cfg_.instrument);
    if (!q || !q->valid) return;

    const double age_ms = static_cast<double>((now - q->ts).count()) / 1'000'000.0;
    if (age_ms > cfg_.stale_quote_ms) {
        ++stats_.quotes_stale_dropped;
        return;
    }

    if (last_quote_ts_.count() > 0 && q->ts <= last_quote_ts_) {
        ++stats_.quotes_dedup_dropped;
        return;
    }
    last_quote_ts_ = q->ts;

    MarketEvent event;
    event.instrument = cfg_.instrument;
    event.source = cfg_.source;
    event.type = MarketEventType::Quote;
    event.exchange_timestamp = q->ts;
    event.provider_timestamp = q->ts;
    event.receive_timestamp = now.count() > 0 ? now : now_utc_ns();
    event.bid = q->bid;
    event.ask = q->ask;
    event.last = (q->bid + q->ask) * 0.5;
    event.sequence = ++quote_seq_;

    // CLOSED 10s one-shot is produced by the candle engine inside process_event.
    // RAW quote path never updates structure.
    pipeline_.process_event(event);
    ++stats_.quotes_processed;
}

void LiveMultiClockPath::poll_authority(Timeframe tf, Timestamp now, std::uint64_t& counter) {
    auto bars = source_.fetch_closed_ohlc(cfg_.epic, tf, cfg_.max_authority_bars);
    if (bars.empty()) return;

    const auto key = static_cast<std::uint32_t>(tf);

    for (const auto& bar : bars) {
        if (!is_fully_closed_bar(bar, tf, now)) continue;
        auto it = last_authority_open_.find(key);
        // Dedup + reject out-of-order (open_time == 0 is a valid first bar).
        if (it != last_authority_open_.end() && bar.time <= it->second) continue;

        Candle c = to_closed_candle(bar, cfg_.instrument);
        pipeline_.process_authority_ohlc(c, tf);
        last_authority_open_[key] = bar.time;
        ++counter;
    }
}

void LiveMultiClockPath::poll_equity() {
    auto eq = source_.fetch_equity();
    if (!eq || !(*eq > 0.0)) {
        ++stats_.equity_missing;
        pipeline_.clear_account_equity();
        return;
    }
    pipeline_.set_account_equity(*eq);
    ++stats_.equity_updates;
}

void LiveMultiClockPath::poll_once(Timestamp now) {
    if (now.count() <= 0) now = now_utc_ns();
    const auto steady = now_steady_ns();

    if (!ensure_connected()) {
        pipeline_.clear_account_equity();
        return;
    }

    if (last_quote_poll_.count() == 0 ||
        (steady - last_quote_poll_).count() >=
            static_cast<long long>(cfg_.quote_poll_ms) * 1'000'000LL) {
        poll_quote(now);
        last_quote_poll_ = steady;
    }

    if (last_1m_poll_.count() == 0 ||
        (steady - last_1m_poll_).count() >=
            static_cast<long long>(cfg_.authority_1m_poll_ms) * 1'000'000LL) {
        poll_authority(Timeframe::Minute1, now, stats_.authority_1m_ingested);
        last_1m_poll_ = steady;
    }

    if (cfg_.poll_higher_timeframes &&
        (last_higher_poll_.count() == 0 ||
         (steady - last_higher_poll_).count() >=
             static_cast<long long>(cfg_.higher_tf_poll_ms) * 1'000'000LL)) {
        poll_authority(Timeframe::Minute5, now, stats_.authority_5m_ingested);
        poll_authority(Timeframe::Minute15, now, stats_.authority_15m_ingested);
        poll_authority(Timeframe::Hour1, now, stats_.authority_1h_ingested);
        last_higher_poll_ = steady;
    }

    if (last_equity_poll_.count() == 0 ||
        (steady - last_equity_poll_).count() >=
            static_cast<long long>(cfg_.equity_poll_ms) * 1'000'000LL) {
        poll_equity();
        last_equity_poll_ = steady;
    }
}

void LiveMultiClockPath::run(std::atomic<bool>& running) {
    while (running.load()) {
        poll_once();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

}  // namespace mr
