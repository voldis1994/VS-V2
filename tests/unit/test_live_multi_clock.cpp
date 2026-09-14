#include <gtest/gtest.h>
#include "mr/market_core/live_multi_clock_path.hpp"
#include "mr/market_core/pipeline.hpp"
#include "mr/candle_engine/candle_engine.hpp"
#include "mr/candle_engine/candle_deduplicator.hpp"

using namespace mr;

namespace {

class FakeCapitalSource final : public ICapitalMarketSource {
public:
    bool session_ok{true};
    std::uint64_t reconnects{0};
    std::optional<CapitalQuote> next_quote{};
    CapitalPriceHistory bars_1m{};
    CapitalPriceHistory bars_5m{};
    CapitalPriceHistory bars_15m{};
    CapitalPriceHistory bars_1h{};
    std::optional<double> equity{10000.0};
    int ensure_calls{0};

    bool ensure_session() override {
        ++ensure_calls;
        return session_ok;
    }
    void invalidate_session() override { session_ok = false; }
    std::optional<CapitalQuote> fetch_quote(InstrumentId) override { return next_quote; }
    CapitalPriceHistory fetch_closed_ohlc(const std::string&, Timeframe tf, int) override {
        switch (tf) {
            case Timeframe::Minute1: return bars_1m;
            case Timeframe::Minute5: return bars_5m;
            case Timeframe::Minute15: return bars_15m;
            case Timeframe::Hour1: return bars_1h;
            default: return {};
        }
    }
    std::optional<double> fetch_equity() override { return equity; }
    std::uint64_t reconnect_count() const override { return reconnects; }
};

}  // namespace

TEST(LiveMultiClock, RawQuoteDoesNotGrantStructureAuthority) {
    MarketCorePipeline pipeline;
    ConfigRegistry config;
    pipeline.configure(config);

    FakeCapitalSource src;
    CapitalQuote q;
    q.bid = 2000.0;
    q.ask = 2000.2;
    q.ts = Timestamp(1'000'000'000LL);
    q.valid = true;
    src.next_quote = q;

    LiveFeedConfig cfg;
    cfg.quote_poll_ms = 0;
    cfg.authority_1m_poll_ms = 999999;
    cfg.equity_poll_ms = 999999;
    cfg.poll_higher_timeframes = false;
    LiveMultiClockPath path(pipeline, src, cfg);

    path.poll_once(Timestamp(1'100'000'000LL));
    EXPECT_GE(path.stats().quotes_processed, 1u);
    EXPECT_FALSE(pipeline.structure().has_authority());
}

TEST(LiveMultiClock, Authority1mUpdatesStructure) {
    MarketCorePipeline pipeline;
    ConfigRegistry config;
    pipeline.configure(config);

    FakeCapitalSource src;
    CapitalPriceBar bar;
    bar.time = Timestamp(0);
    bar.open = 2000;
    bar.high = 2010;
    bar.low = 1995;
    bar.close = 2005;
    src.bars_1m = {bar};
    src.next_quote = std::nullopt;

    LiveFeedConfig cfg;
    cfg.quote_poll_ms = 999999;
    cfg.authority_1m_poll_ms = 0;
    cfg.equity_poll_ms = 999999;
    cfg.poll_higher_timeframes = false;
    LiveMultiClockPath path(pipeline, src, cfg);

    path.poll_once(Timestamp(60'000'000'000LL));
    EXPECT_GE(path.stats().authority_1m_ingested, 1u);
    EXPECT_TRUE(pipeline.structure().has_authority());
}

TEST(LiveMultiClock, DuplicateAndOutOfOrderAuthorityIgnored) {
    MarketCorePipeline pipeline;
    ConfigRegistry config;
    pipeline.configure(config);

    FakeCapitalSource src;
    CapitalPriceBar a;
    a.time = Timestamp(0);
    a.open = 1;
    a.high = 2;
    a.low = 0.5;
    a.close = 1.5;
    src.bars_1m = {a};

    LiveFeedConfig cfg;
    cfg.quote_poll_ms = 999999;
    cfg.authority_1m_poll_ms = 0;
    cfg.equity_poll_ms = 999999;
    cfg.poll_higher_timeframes = false;
    LiveMultiClockPath path(pipeline, src, cfg);

    path.poll_once(Timestamp(120'000'000'000LL));
    EXPECT_EQ(path.stats().authority_1m_ingested, 1u);

    CapitalPriceBar older = a;
    older.time = Timestamp(-1);
    src.bars_1m = {a, older};
    path.poll_once(Timestamp(180'000'000'000LL));
    EXPECT_EQ(path.stats().authority_1m_ingested, 1u);
}

TEST(LiveMultiClock, ReconnectOnSessionFailure) {
    MarketCorePipeline pipeline;
    ConfigRegistry config;
    pipeline.configure(config);

    FakeCapitalSource src;
    src.session_ok = false;

    LiveFeedConfig cfg;
    LiveMultiClockPath path(pipeline, src, cfg);
    path.poll_once(Timestamp(1'000'000'000LL));
    EXPECT_GE(path.stats().session_failures, 1u);
    EXPECT_GE(path.stats().reconnect_attempts, 1u);

    src.session_ok = true;
    path.poll_once(Timestamp(2'000'000'000LL));
    EXPECT_GE(src.ensure_calls, 2);
}

TEST(LiveMultiClock, StaleAndDedupQuotesDropped) {
    MarketCorePipeline pipeline;
    ConfigRegistry config;
    pipeline.configure(config);

    FakeCapitalSource src;
    CapitalQuote q;
    q.bid = 10;
    q.ask = 10.1;
    q.ts = Timestamp(1'000'000'000LL);
    q.valid = true;
    src.next_quote = q;

    LiveFeedConfig cfg;
    cfg.stale_quote_ms = 500;
    cfg.quote_poll_ms = 0;
    cfg.authority_1m_poll_ms = 999999;
    cfg.equity_poll_ms = 999999;
    cfg.poll_higher_timeframes = false;
    LiveMultiClockPath path(pipeline, src, cfg);

    path.poll_once(Timestamp(3'000'000'000LL));
    EXPECT_GE(path.stats().quotes_stale_dropped, 1u);

    q.ts = Timestamp(3'000'000'000LL);
    src.next_quote = q;
    path.poll_once(Timestamp(3'100'000'000LL));
    EXPECT_GE(path.stats().quotes_processed, 1u);

    path.poll_once(Timestamp(3'200'000'000LL));
    EXPECT_GE(path.stats().quotes_dedup_dropped, 1u);
}

TEST(LiveMultiClock, RealEquityWiredMissingClears) {
    MarketCorePipeline pipeline;
    ConfigRegistry config;
    pipeline.configure(config);

    FakeCapitalSource src;
    src.equity = 25000.0;
    src.next_quote = std::nullopt;

    LiveFeedConfig cfg;
    cfg.quote_poll_ms = 999999;
    cfg.authority_1m_poll_ms = 999999;
    cfg.equity_poll_ms = 0;
    cfg.poll_higher_timeframes = false;
    LiveMultiClockPath path(pipeline, src, cfg);

    path.poll_once(Timestamp(1'000'000'000LL));
    EXPECT_TRUE(pipeline.has_account_equity());
    EXPECT_GE(path.stats().equity_updates, 1u);

    src.equity = std::nullopt;
    path.poll_once(Timestamp(2'000'000'000LL));
    EXPECT_FALSE(pipeline.has_account_equity());
    EXPECT_GE(path.stats().equity_missing, 1u);
}

TEST(LiveMultiClock, HigherTimeframeAuthorityPathPrepared) {
    MarketCorePipeline pipeline;
    ConfigRegistry config;
    pipeline.configure(config);

    FakeCapitalSource src;
    CapitalPriceBar bar;
    bar.time = Timestamp(0);
    bar.open = 100;
    bar.high = 110;
    bar.low = 90;
    bar.close = 105;
    src.bars_5m = {bar};
    src.bars_15m = {bar};
    src.bars_1h = {bar};

    LiveFeedConfig cfg;
    cfg.quote_poll_ms = 999999;
    cfg.authority_1m_poll_ms = 999999;
    cfg.equity_poll_ms = 999999;
    cfg.higher_tf_poll_ms = 0;
    cfg.poll_higher_timeframes = true;
    LiveMultiClockPath path(pipeline, src, cfg);

    path.poll_once(Timestamp(3'600'000'000'000LL));
    EXPECT_GE(path.stats().authority_5m_ingested, 1u);
    EXPECT_GE(path.stats().authority_15m_ingested, 1u);
    EXPECT_GE(path.stats().authority_1h_ingested, 1u);
}

TEST(TenSecondOneShot, EmittedOncePerBucket) {
    CandleEngine eng;
    std::size_t closes = 0;
    for (int i = 0; i < 5; ++i) {
        auto evs = eng.on_quote(100.0 + i * 0.1, Timestamp(static_cast<long long>(i) * 1'000'000'000LL), 1);
        for (const auto& e : evs) {
            if (e.kind == MarketClockKind::ClosedTenSecond) ++closes;
        }
    }
    EXPECT_EQ(closes, 0u);
    auto evs = eng.on_quote(101.0, Timestamp(10'000'000'000LL), 1);
    std::size_t shot = 0;
    for (const auto& e : evs) {
        if (e.kind == MarketClockKind::ClosedTenSecond) {
            EXPECT_TRUE(e.one_shot);
            EXPECT_FALSE(e.structure_authority);
            ++shot;
        }
        if (e.kind == MarketClockKind::ClosedOneMinute) {
            EXPECT_FALSE(e.structure_authority);
        }
    }
    EXPECT_EQ(shot, 1u);
}

TEST(CandleDedup, RejectsDuplicateAndOutOfOrder) {
    CandleDeduplicator d;
    Candle c;
    c.open_time = Timestamp(10);
    c.open = c.high = c.low = c.close = 1;
    c.ticks = 1;
    EXPECT_TRUE(d.accept(c));
    EXPECT_FALSE(d.accept(c));
    Candle older = c;
    older.open_time = Timestamp(5);
    EXPECT_FALSE(d.accept(older));
    Candle newer = c;
    newer.open_time = Timestamp(20);
    EXPECT_TRUE(d.accept(newer));
}
