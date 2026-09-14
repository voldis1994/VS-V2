#include <gtest/gtest.h>

#include "mr/microstructure_engine/microstructure_engine.hpp"
#include "mr/structure_engine/structure_engine.hpp"
#include "mr/market_core/pipeline.hpp"
#include "mr/brain/brain_state.hpp"
#include "mr/market_types/market_event.hpp"
#include "mr/perception_engine/price_dynamics.hpp"

#include <vector>

using namespace mr;

namespace {

Candle make_10s(std::int64_t open_ns, double o, double h, double l, double c,
                InstrumentId inst = 1) {
    Candle bar;
    bar.instrument = inst;
    bar.open_time = Timestamp(open_ns);
    bar.open = o;
    bar.high = h;
    bar.low = l;
    bar.close = c;
    bar.ticks = 5;
    bar.status = CandleStatus::Closed;
    return bar;
}

constexpr std::int64_t kStep = 10'000'000'000LL;

std::vector<Candle> bullish_sequence() {
    return {
        make_10s(0 * kStep, 100, 101, 99.8, 100.6),
        make_10s(1 * kStep, 100.6, 102.5, 100.5, 102.2),
        make_10s(2 * kStep, 102.2, 102.4, 101.5, 101.8),
        make_10s(3 * kStep, 101.8, 102.0, 101.0, 101.2),
        make_10s(4 * kStep, 101.2, 103.5, 101.1, 103.2),
        make_10s(5 * kStep, 103.2, 103.4, 102.5, 102.8),
        make_10s(6 * kStep, 102.8, 103.0, 102.0, 102.3),
        make_10s(7 * kStep, 102.3, 104.5, 102.2, 104.2),
    };
}

}  // namespace

TEST(MicrostructureBrain, Closed10sUpdatesGeometryAndMomentum) {
    MicrostructureEngine micro;
    EXPECT_FALSE(micro.has_authority());
    for (const auto& b : bullish_sequence()) micro.on_closed_10s(b);
    const auto f = micro.snapshot();
    EXPECT_TRUE(f.has_authority);
    EXPECT_TRUE(f.setup_confirmed);
    EXPECT_EQ(f.closed_10s_count, 8u);
    EXPECT_NE(f.body_pct, 0.0);
    EXPECT_GE(f.candle_strength, 0.0);
    EXPECT_LE(f.candle_strength, 1.0);
    EXPECT_GE(f.upper_wick_pct, 0.0);
    EXPECT_GE(f.lower_wick_pct, 0.0);
    EXPECT_NE(f.momentum, 0.0);
}

TEST(MicrostructureBrain, BuyerSellerPressureAndContinuation) {
    MicrostructureEngine micro;
    for (const auto& b : bullish_sequence()) micro.on_closed_10s(b);
    const auto f = micro.snapshot();
    EXPECT_GT(f.buyer_pressure, 0.0);
    EXPECT_GT(f.seller_pressure, 0.0);
    EXPECT_GE(f.continuation + f.exhaustion, 0.0);
    EXPECT_GE(f.entry_timing_quality, 0.0);
    EXPECT_LE(f.entry_timing_quality, 1.0);
}

TEST(MicrostructureBrain, LocalSwingsAndPullbackEvidence) {
    MicrostructureEngine micro;
    std::vector<Candle> bars = {
        make_10s(0 * kStep, 100, 102, 100, 101),
        make_10s(1 * kStep, 101, 110, 101, 108),
        make_10s(2 * kStep, 108, 108, 104, 105),
        make_10s(3 * kStep, 105, 107, 103, 104),
        make_10s(4 * kStep, 104, 109, 104, 107),
        make_10s(5 * kStep, 107, 115, 107, 113),
        make_10s(6 * kStep, 113, 113, 110, 111),
        make_10s(7 * kStep, 111, 112, 106, 108),
        make_10s(8 * kStep, 108, 114, 107, 112),
        make_10s(9 * kStep, 112, 112.2, 107.8, 108.2),
    };
    for (const auto& b : bars) micro.on_closed_10s(b);
    const auto f = micro.snapshot();
    EXPECT_GE(f.pullback_depth, 0.0);
    EXPECT_LE(f.pullback_depth, 1.0);
    EXPECT_GE(f.pullback_quality, 0.0);
}

TEST(MicrostructureBrain, BreakoutAndFailedBreakout) {
    MicrostructureEngine micro;
    for (int i = 0; i < 8; ++i) {
        if ((i % 2) == 0) micro.on_closed_10s(make_10s(i * kStep, 100, 101, 99.8, 100.5));
        else micro.on_closed_10s(make_10s(i * kStep, 100.5, 100.7, 99.5, 99.8));
    }
    micro.on_closed_10s(make_10s(8 * kStep, 100.5, 103.5, 100.4, 103.2));
    auto f = micro.snapshot();
    EXPECT_TRUE(f.breakout_up || f.breakout_strength > 0.0);

    micro.on_closed_10s(make_10s(9 * kStep, 103.0, 103.1, 99.7, 100.1));
    f = micro.snapshot();
    EXPECT_TRUE(f.failed_breakout_up || f.failed_breakout > 0.0);
}

TEST(MicrostructureBrain, CompressionAndExpansion) {
    MicrostructureEngine micro;
    for (int i = 0; i < 10; ++i) {
        micro.on_closed_10s(make_10s(i * kStep, 100, 101.0, 99.0, 100.2));
    }
    for (int i = 10; i < 13; ++i) {
        micro.on_closed_10s(make_10s(i * kStep, 100.2, 100.35, 100.10, 100.25));
    }
    auto f = micro.snapshot();
    EXPECT_GT(f.compression, 0.0);

    micro.on_closed_10s(make_10s(13 * kStep, 100.25, 103.5, 99.5, 103.0));
    micro.on_closed_10s(make_10s(14 * kStep, 103.0, 105.0, 102.0, 104.5));
    micro.on_closed_10s(make_10s(15 * kStep, 104.5, 107.0, 104.0, 106.5));
    f = micro.snapshot();
    EXPECT_GT(f.expansion, 0.0);
    EXPECT_GT(f.volatility, 0.0);
}

TEST(MicrostructureBrain, OneShotSameOpenTimeIgnored) {
    MicrostructureEngine micro;
    const auto a = make_10s(0, 100, 101, 99, 100.5);
    micro.on_closed_10s(a);
    EXPECT_EQ(micro.closed_10s_count(), 1u);
    const auto before = micro.snapshot();

    Candle dup = a;
    dup.close = 999;
    dup.high = 1000;
    micro.on_closed_10s(dup);
    EXPECT_EQ(micro.closed_10s_count(), 1u);
    EXPECT_DOUBLE_EQ(micro.snapshot().body_pct, before.body_pct);
}

TEST(MicrostructureBrain, RawQuoteDoesNotConfirmSetup) {
    MicrostructureEngine micro;
    NormalizedEvent e;
    e.type = MarketEventType::Quote;
    e.bid = 100.0;
    e.ask = 100.2;
    e.bid_size = 2.0;
    e.ask_size = 1.0;
    e.last = 100.1;
    micro.update(e);

    const auto f = micro.snapshot();
    EXPECT_FALSE(f.has_authority);
    EXPECT_FALSE(f.setup_confirmed);
    EXPECT_EQ(f.closed_10s_count, 0u);
    EXPECT_GT(f.spread, 0.0);
}

TEST(MicrostructureBrain, PipelineRawDoesNotConfirmMicro) {
    MarketCorePipeline pipeline;
    ConfigRegistry config;
    pipeline.configure(config);

    MarketEvent quote;
    quote.instrument = 1;
    quote.source = 1;
    quote.type = MarketEventType::Quote;
    quote.exchange_timestamp = Timestamp(1'000'000'000LL);
    quote.receive_timestamp = quote.exchange_timestamp;
    quote.bid = 2000.0;
    quote.ask = 2000.2;
    quote.last = 2000.1;
    quote.sequence = 1;
    pipeline.process_event(quote);

    EXPECT_FALSE(pipeline.micro().has_authority());
    EXPECT_FALSE(pipeline.micro().snapshot().setup_confirmed);
}

TEST(MicrostructureBrain, FormingCandleDoesNotConfirmMicro) {
    // Quotes inside one 10s bucket emit FormingCandle only — never micro authority.
    MarketCorePipeline pipeline;
    ConfigRegistry config;
    pipeline.configure(config);

    for (int i = 0; i < 5; ++i) {
        MarketEvent quote;
        quote.instrument = 1;
        quote.source = 1;
        quote.type = MarketEventType::Quote;
        quote.exchange_timestamp = Timestamp(1'000'000'000LL + i * 100'000'000LL);
        quote.receive_timestamp = quote.exchange_timestamp;
        quote.bid = 2000.0 + i * 0.1;
        quote.ask = 2000.2 + i * 0.1;
        quote.last = 2000.1 + i * 0.1;
        quote.sequence = static_cast<std::uint64_t>(i + 1);
        pipeline.process_event(quote);
    }

    EXPECT_FALSE(pipeline.micro().has_authority());
    EXPECT_FALSE(pipeline.micro().snapshot().setup_confirmed);
    EXPECT_EQ(pipeline.micro().closed_10s_count(), 0u);
    EXPECT_EQ(pipeline.structure().authority_updates(), 0u);
}

TEST(MicrostructureBrain, Closed10sDoesNotMutateStructureEngine) {
    StructureEngine structure;
    MicrostructureEngine micro;
    PriceDynamics pd;

    Candle m1;
    m1.instrument = 1;
    m1.open_time = Timestamp(0);
    m1.open = 100;
    m1.high = 110;
    m1.low = 99;
    m1.close = 108;
    m1.status = CandleStatus::Closed;
    structure.on_authority_close(m1, Timeframe::Minute1, pd);
    ASSERT_TRUE(structure.has_authority());
    const auto st_before = structure.snapshot();
    const auto updates = structure.authority_updates();

    for (const auto& b : bullish_sequence()) micro.on_closed_10s(b);

    EXPECT_TRUE(micro.has_authority());
    EXPECT_TRUE(micro.snapshot().setup_confirmed);
    EXPECT_EQ(structure.authority_updates(), updates);
    EXPECT_EQ(structure.snapshot().trend_direction, st_before.trend_direction);
    EXPECT_DOUBLE_EQ(structure.snapshot().swing_state, st_before.swing_state);
}

TEST(MicrostructureBrain, BrainStateMicroPreservesStructure) {
    BrainState state;
    StructureFeatures st;
    st.trend_direction = TrendBias::Up;
    st.swing_state = 1.0;
    st.trend_strength = 0.7;
    RegimeFeatures rg;
    rg.current = Regime::TrendUp;
    state.apply_authority(1, st, rg, Timestamp(100));

    MicrostructureEngine micro;
    for (const auto& b : bullish_sequence()) micro.on_closed_10s(b);
    state.apply_micro(1, micro.snapshot(), Timestamp(200));

    const auto& ctx = state.latest().instruments.at(1);
    EXPECT_TRUE(ctx.has_structure_authority);
    EXPECT_TRUE(ctx.has_micro_authority);
    EXPECT_EQ(ctx.structure.trend_direction, TrendBias::Up);
    EXPECT_DOUBLE_EQ(ctx.structure.swing_state, 1.0);
    EXPECT_TRUE(ctx.micro.setup_confirmed);
    EXPECT_GE(ctx.micro.closed_10s_count, 1u);

    // Quote-path update must not wipe either authority.
    BrainContext q;
    q.instrument = 1;
    q.ts = Timestamp(300);
    state.update(q);
    const auto& ctx2 = state.latest().instruments.at(1);
    EXPECT_TRUE(ctx2.has_structure_authority);
    EXPECT_TRUE(ctx2.has_micro_authority);
    EXPECT_EQ(ctx2.structure.trend_direction, TrendBias::Up);
    EXPECT_TRUE(ctx2.micro.setup_confirmed);
}

TEST(MicrostructureBrain, RejectionAcceptanceReclaimAreContinuous) {
    MicrostructureEngine micro;
    micro.on_closed_10s(make_10s(0, 100, 101, 99, 100.5));
    micro.on_closed_10s(make_10s(kStep, 100.5, 103, 100.4, 100.6));
    auto f = micro.snapshot();
    EXPECT_GE(f.rejection + f.acceptance, 0.0);

    micro.on_closed_10s(make_10s(2 * kStep, 100.6, 102.5, 100.5, 102.3));
    f = micro.snapshot();
    EXPECT_GE(f.reclaim, 0.0);
    EXPECT_GE(f.rejection_proxy, 0.0);
    EXPECT_GE(f.reclaim_proxy, 0.0);
}

TEST(MicrostructureBrain, AccelerationDecelerationFromSequence) {
    MicrostructureEngine micro;
    micro.on_closed_10s(make_10s(0, 100, 100.5, 99.5, 100.2));
    micro.on_closed_10s(make_10s(kStep, 100.2, 101.0, 100.1, 100.8));
    micro.on_closed_10s(make_10s(2 * kStep, 100.8, 103.0, 100.7, 102.8));  // accel
    auto f = micro.snapshot();
    EXPECT_GE(f.acceleration + f.deceleration, 0.0);

    micro.on_closed_10s(make_10s(3 * kStep, 102.8, 103.0, 102.5, 102.6));  // slow
    f = micro.snapshot();
    EXPECT_GE(f.deceleration + f.acceleration, 0.0);
}

TEST(MicrostructureBrain, EvidenceIsContinuousWithoutHardcodedBehaviorGates) {
    // Gradual vol contraction → continuous compression (no cliff at 0.65).
    MicrostructureEngine micro;
    for (int i = 0; i < 8; ++i) {
        micro.on_closed_10s(make_10s(i * kStep, 100, 102.0, 98.0, 100.5));
    }
    const double wide_comp = micro.snapshot().compression;

    for (int i = 8; i < 12; ++i) {
        const double w = 1.5 - 0.25 * (i - 8);  // steadily tighter
        micro.on_closed_10s(make_10s(i * kStep, 100.2, 100.2 + w, 100.2 - w, 100.3));
    }
    const double mid_comp = micro.snapshot().compression;

    for (int i = 12; i < 15; ++i) {
        micro.on_closed_10s(make_10s(i * kStep, 100.3, 100.38, 100.22, 100.32));
    }
    const double tight_comp = micro.snapshot().compression;

    EXPECT_GE(mid_comp, wide_comp);
    EXPECT_GE(tight_comp, mid_comp);
    EXPECT_GT(tight_comp, 0.0);

    // Gradual momentum increase → continuous continuation (no if>X trigger).
    MicrostructureEngine mom;
    mom.on_closed_10s(make_10s(0, 100, 100.2, 99.9, 100.1));
    mom.on_closed_10s(make_10s(kStep, 100.1, 100.4, 100.0, 100.25));
    const double c1 = mom.snapshot().continuation;
    mom.on_closed_10s(make_10s(2 * kStep, 100.25, 101.2, 100.2, 101.0));
    const double c2 = mom.snapshot().continuation;
    mom.on_closed_10s(make_10s(3 * kStep, 101.0, 103.0, 100.9, 102.8));
    const double c3 = mom.snapshot().continuation;
    EXPECT_GE(c2, c1);
    EXPECT_GE(c3, c2);
}

TEST(MicrostructureBrain, MicroNormConfigIsStage8Calibratable) {
    auto cfg_hi = MicroNormConfig::defaults();
    cfg_hi.compression_scale = 2.0;
    cfg_hi.continuation_scale = 2.0;
    cfg_hi.momentum_scale = 0.001;

    auto cfg_lo = MicroNormConfig::defaults();
    cfg_lo.compression_scale = 0.5;
    cfg_lo.continuation_scale = 0.5;
    cfg_lo.momentum_scale = 0.01;

    MicrostructureEngine hi(cfg_hi);
    MicrostructureEngine lo(cfg_lo);

    auto feed = [](MicrostructureEngine& m) {
        for (int i = 0; i < 10; ++i) {
            m.on_closed_10s(make_10s(i * kStep, 100, 101.5, 98.5, 100.4));
        }
        for (int i = 10; i < 14; ++i) {
            m.on_closed_10s(make_10s(i * kStep, 100.4, 100.5, 100.3, 100.45));
        }
        m.on_closed_10s(make_10s(14 * kStep, 100.45, 102.5, 100.4, 102.2));
    };
    feed(hi);
    feed(lo);

    EXPECT_GT(hi.snapshot().compression, lo.snapshot().compression);
    EXPECT_NE(hi.snapshot().continuation, lo.snapshot().continuation);
    EXPECT_TRUE(hi.snapshot().setup_confirmed);
    EXPECT_TRUE(lo.snapshot().setup_confirmed);
    // Config changes evidence scale — never a BUY/SELL gate.
    EXPECT_GE(hi.snapshot().entry_timing_quality, 0.0);
    EXPECT_LE(hi.snapshot().entry_timing_quality, 1.0);
}
