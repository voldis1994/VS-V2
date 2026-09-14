#include <gtest/gtest.h>

#include "mr/structure_engine/structure_engine.hpp"
#include "mr/market_concepts/market_concepts_engine.hpp"
#include "mr/market_core/pipeline.hpp"
#include "mr/market_types/market_event.hpp"

#include <cmath>
#include <vector>

using namespace mr;

namespace {

Candle make_bar(std::int64_t open_ns, double o, double h, double l, double c,
                InstrumentId inst = 1) {
    Candle bar;
    bar.instrument = inst;
    bar.open_time = Timestamp(open_ns);
    bar.open = o;
    bar.high = h;
    bar.low = l;
    bar.close = c;
    bar.ticks = 10;
    bar.status = CandleStatus::Closed;
    return bar;
}

void feed(StructureEngine& eng, Timeframe tf, const std::vector<Candle>& bars) {
    PriceDynamics pd;
    for (const auto& b : bars) {
        eng.on_authority_close(b, tf, pd);
    }
}

/** Clear HH/HL uptrend zigzag (pivot L=1,R=1). */
std::vector<Candle> uptrend_bars(std::int64_t step_ns = 60'000'000'000LL) {
    // Indices: 0 1 2 3 4 5 6 7 8
    // Swing high @1 (110), low @3 (103), high @5 (115=HH), low @7 (106=HL)
    return {
        make_bar(0 * step_ns, 100, 102, 100, 101),
        make_bar(1 * step_ns, 101, 110, 101, 108),
        make_bar(2 * step_ns, 108, 108, 104, 105),
        make_bar(3 * step_ns, 105, 107, 103, 104),
        make_bar(4 * step_ns, 104, 109, 104, 107),
        make_bar(5 * step_ns, 107, 115, 107, 113),
        make_bar(6 * step_ns, 113, 113, 110, 111),
        make_bar(7 * step_ns, 111, 112, 106, 108),
        make_bar(8 * step_ns, 108, 114, 107, 112),
    };
}

/** Clear LH/LL downtrend zigzag. */
std::vector<Candle> downtrend_bars(std::int64_t step_ns = 60'000'000'000LL) {
    // Swing low @1 (90), high @3 (97), low @5 (85=LL), high @7 (94=LH)
    return {
        make_bar(0 * step_ns, 100, 100, 98, 99),
        make_bar(1 * step_ns, 99, 99, 90, 92),
        make_bar(2 * step_ns, 92, 96, 92, 95),
        make_bar(3 * step_ns, 95, 97, 93, 94),
        make_bar(4 * step_ns, 94, 94, 91, 92),
        make_bar(5 * step_ns, 92, 92, 85, 87),
        make_bar(6 * step_ns, 87, 91, 87, 90),
        make_bar(7 * step_ns, 90, 94, 89, 91),
        make_bar(8 * step_ns, 91, 91, 88, 89),
    };
}

/** Sideways range oscillation. */
std::vector<Candle> range_bars(std::int64_t step_ns = 60'000'000'000LL) {
    std::vector<Candle> out;
    for (int i = 0; i < 12; ++i) {
        const bool up = (i % 2) == 0;
        const double base = 100.0;
        if (up) {
            out.push_back(make_bar(i * step_ns, base, base + 1.0, base - 0.2, base + 0.6));
        } else {
            out.push_back(make_bar(i * step_ns, base + 0.6, base + 0.8, base - 1.0, base - 0.4));
        }
    }
    return out;
}

}  // namespace

TEST(StructureBrain, UptrendDetectsHhHlAndTrendUp) {
    StructureEngine eng;
    feed(eng, Timeframe::Minute1, uptrend_bars());
    const auto st = eng.snapshot();
    EXPECT_TRUE(eng.has_authority());
    EXPECT_GE(st.swing_count, 2u);
    EXPECT_EQ(st.trend_direction, TrendBias::Up);
    EXPECT_GT(st.trend_strength, 0.0);
    EXPECT_GT(st.trend_age, 0u);
    EXPECT_GT(st.structure_quality, 0.0);
    EXPECT_DOUBLE_EQ(st.swing_state, 1.0);
}

TEST(StructureBrain, DowntrendDetectsLhLlAndTrendDown) {
    StructureEngine eng;
    feed(eng, Timeframe::Minute1, downtrend_bars());
    const auto st = eng.snapshot();
    EXPECT_EQ(st.trend_direction, TrendBias::Down);
    EXPECT_GT(st.trend_strength, 0.0);
    EXPECT_DOUBLE_EQ(st.swing_state, -1.0);
}

TEST(StructureBrain, PullbackDepthInUptrend) {
    StructureEngine eng;
    auto bars = uptrend_bars();
    // Deep pullback bar still above last HL (~106): close near mid of swing.
    bars.push_back(make_bar(9 * 60'000'000'000LL, 112, 112.5, 107.5, 108.0));
    feed(eng, Timeframe::Minute1, bars);
    const auto st = eng.snapshot();
    EXPECT_EQ(st.trend_direction, TrendBias::Up);
    EXPECT_TRUE(st.in_pullback);
    EXPECT_GT(st.pullback_depth, 0.15);
}

TEST(StructureBrain, RangePositionInsideOscillation) {
    StructureEngine eng;
    feed(eng, Timeframe::Minute1, range_bars());
    const auto st = eng.snapshot();
    EXPECT_TRUE(st.in_range || st.trend_direction == TrendBias::Range);
    EXPECT_GE(st.range_position, 0.0);
    EXPECT_LE(st.range_position, 1.0);
    EXPECT_GT(st.range_width, 0.0);
}

TEST(StructureBrain, BreakoutAndFailedBreakout) {
    StructureEngine eng;
    auto bars = range_bars();
    // Breakout above range highs (~101).
    bars.push_back(make_bar(12 * 60'000'000'000LL, 100.5, 103.5, 100.4, 103.2));
    feed(eng, Timeframe::Minute1, bars);
    auto st = eng.snapshot();
    EXPECT_TRUE(st.breakout_up || st.breakout_active);
    EXPECT_GT(st.breakout_strength, 0.0);

    // Fail: close back inside range.
    PriceDynamics pd;
    eng.on_authority_close(make_bar(13 * 60'000'000'000LL, 103.0, 103.1, 99.8, 100.2),
                           Timeframe::Minute1, pd);
    st = eng.snapshot();
    EXPECT_TRUE(st.failed_breakout_up || st.failed_breakout > 0.3);
}

TEST(StructureBrain, ReversalEvidenceOnInvalidation) {
    StructureEngine eng;
    auto bars = uptrend_bars();
    // Smash below last HL (106) → structural invalidation / reversal evidence.
    bars.push_back(make_bar(9 * 60'000'000'000LL, 108, 108.5, 100.0, 101.0));
    feed(eng, Timeframe::Minute1, bars);
    const auto st = eng.snapshot();
    EXPECT_GT(st.structural_invalidation, 0.0);
    EXPECT_GT(st.reversal_candidate, 0.0);
}

TEST(StructureBrain, CompressionThenExpansion) {
    StructureEngine eng;
    std::vector<Candle> bars;
    // Long baseline with moderate ranges.
    for (int i = 0; i < 12; ++i) {
        bars.push_back(make_bar(i * 60'000'000'000LL, 100, 101.0, 99.0, 100.2));
    }
    // Compression: tiny ranges.
    for (int i = 12; i < 16; ++i) {
        bars.push_back(make_bar(i * 60'000'000'000LL, 100.2, 100.35, 100.10, 100.25));
    }
    feed(eng, Timeframe::Minute1, bars);
    auto st = eng.snapshot();
    EXPECT_GT(st.compression, 0.0);

    // Expansion burst.
    PriceDynamics pd;
    eng.on_authority_close(make_bar(16 * 60'000'000'000LL, 100.25, 103.5, 99.5, 103.0),
                           Timeframe::Minute1, pd);
    eng.on_authority_close(make_bar(17 * 60'000'000'000LL, 103.0, 105.0, 102.0, 104.5),
                           Timeframe::Minute1, pd);
    eng.on_authority_close(make_bar(18 * 60'000'000'000LL, 104.5, 107.0, 104.0, 106.5),
                           Timeframe::Minute1, pd);
    eng.on_authority_close(make_bar(19 * 60'000'000'000LL, 106.5, 109.0, 106.0, 108.5),
                           Timeframe::Minute1, pd);
    st = eng.snapshot();
    EXPECT_GT(st.expansion, 0.0);
    EXPECT_GT(st.volatility, 0.0);
}

TEST(StructureBrain, SubMinuteDoesNotMutateStructure) {
    StructureEngine eng;
    feed(eng, Timeframe::Minute1, uptrend_bars());
    const auto before = eng.snapshot();
    const auto updates = eng.authority_updates();

    PriceDynamics pd;
    eng.on_authority_close(make_bar(100, 999, 1000, 998, 999.5), Timeframe::Second10, pd);
    eng.on_authority_close(make_bar(101, 1, 2, 0.5, 1.5), Timeframe::Second1, pd);

    EXPECT_EQ(eng.authority_updates(), updates);
    const auto after = eng.snapshot();
    EXPECT_EQ(after.trend_direction, before.trend_direction);
    EXPECT_DOUBLE_EQ(after.swing_state, before.swing_state);
    EXPECT_DOUBLE_EQ(after.last_swing_high, before.last_swing_high);
}

TEST(StructureBrain, Hierarchy1m5m15m1h) {
    StructureEngine eng;
    // 1m pullback-ish while HTF uptrend.
    feed(eng, Timeframe::Hour1, uptrend_bars(3'600'000'000'000LL));
    feed(eng, Timeframe::Minute15, uptrend_bars(900'000'000'000LL));
    feed(eng, Timeframe::Minute5, uptrend_bars(300'000'000'000LL));

    auto m1 = uptrend_bars();
    m1.push_back(make_bar(9 * 60'000'000'000LL, 112, 112.2, 107.8, 108.2));
    feed(eng, Timeframe::Minute1, m1);

    const auto h1 = eng.snapshot(Timeframe::Hour1);
    const auto m15 = eng.snapshot(Timeframe::Minute15);
    const auto m5 = eng.snapshot(Timeframe::Minute5);
    const auto m1s = eng.snapshot(Timeframe::Minute1);
    const auto composite = eng.snapshot();

    EXPECT_EQ(h1.trend_direction, TrendBias::Up);
    EXPECT_EQ(m15.trend_direction, TrendBias::Up);
    EXPECT_EQ(m5.trend_direction, TrendBias::Up);
    EXPECT_GE(m1s.bar_count, 9u);
    // Hierarchical composite keeps HTF trend bias.
    EXPECT_EQ(composite.trend_direction, TrendBias::Up);
}

TEST(StructureBrain, RegimeIsContextNotTrigger) {
    StructureEngine eng;
    MarketConceptsEngine concepts;
    feed(eng, Timeframe::Minute1, uptrend_bars());
    PriceDynamics pd;
    auto rg = concepts.evaluate(pd, eng.snapshot());
    // Multi-concept scores always exist; dominant is descriptive argmax only.
    EXPECT_EQ(rg.scores.size(), kConceptCount);
    EXPECT_GT(rg.score(Regime::TrendUp) + rg.score(Regime::PullbackUptrend)
                  + rg.score(Regime::Expansion) + rg.score(Regime::BreakoutUp),
              0.0);
    // Concepts are classifiers — presence does not imply EntryReady / BUY/SELL.
    EXPECT_NE(regime_name(rg.current), nullptr);
    EXPECT_NE(regime_name(rg.dominant), nullptr);
}

TEST(StructureBrain, AuthorityOnlyMutationViaPipelineAndBrainState) {
    MarketCorePipeline pipeline;
    ConfigRegistry config;
    pipeline.configure(config);

    EXPECT_FALSE(pipeline.structure().has_authority());

    // RAW quote must not grant structure authority or mutate BrainState structure.
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

    EXPECT_FALSE(pipeline.structure().has_authority());
    auto snap0 = pipeline.brain_snapshot();
    if (!snap0.instruments.empty()) {
        const auto& ctx = snap0.instruments.begin()->second;
        EXPECT_FALSE(ctx.has_structure_authority);
    }

    // Authority 1m OHLC sequence establishes structure + BrainState.
    const auto bars = uptrend_bars();
    for (const auto& b : bars) {
        pipeline.process_authority_ohlc(b, Timeframe::Minute1);
    }
    EXPECT_TRUE(pipeline.structure().has_authority());
    EXPECT_EQ(pipeline.structure().snapshot().trend_direction, TrendBias::Up);

    auto snap1 = pipeline.brain_snapshot();
    ASSERT_FALSE(snap1.instruments.empty());
    const auto& ctx1 = snap1.instruments.at(1);
    EXPECT_TRUE(ctx1.has_structure_authority);
    EXPECT_EQ(ctx1.structure.trend_direction, TrendBias::Up);
    const double swing_before = ctx1.structure.swing_state;
    const auto trend_before = ctx1.structure.trend_direction;

    // More quotes must not rewrite BrainState structure.
    quote.sequence = 2;
    quote.exchange_timestamp = Timestamp(2'000'000'000LL);
    quote.receive_timestamp = quote.exchange_timestamp;
    quote.bid = 1500.0;
    quote.ask = 1500.3;
    quote.last = 1500.1;
    pipeline.process_event(quote);

    auto snap2 = pipeline.brain_snapshot();
    const auto& ctx2 = snap2.instruments.at(1);
    EXPECT_TRUE(ctx2.has_structure_authority);
    EXPECT_EQ(ctx2.structure.trend_direction, trend_before);
    EXPECT_DOUBLE_EQ(ctx2.structure.swing_state, swing_before);
}

TEST(StructureBrain, FourteenRegimesAreNamedContextConcepts) {
    // Ensure all 14 regime names exist as market concepts (not trade triggers).
    const Regime all[] = {
        Regime::Unknown,           Regime::Range,           Regime::TrendUp,
        Regime::TrendDown,         Regime::PullbackUptrend, Regime::PullbackDowntrend,
        Regime::Compression,       Regime::Expansion,       Regime::BreakoutUp,
        Regime::BreakoutDown,      Regime::FailedBreakoutUp,Regime::FailedBreakoutDown,
        Regime::ReversalCandidate, Regime::Transition};
    EXPECT_EQ(sizeof(all) / sizeof(all[0]), 14u);
    for (Regime r : all) {
        EXPECT_STRNE(regime_name(r), "");
    }
}

TEST(MarketConcepts, MultipleConceptsCanCoexist) {
    // Uptrend structure with pullback depth + compression mass → several scores > 0.
    StructureFeatures st;
    st.trend_direction = TrendBias::Up;
    st.trend_strength = 0.8;
    st.swing_state = 1.0;
    st.pullback_depth = 0.55;
    st.in_pullback = true;
    st.compression = 0.7;
    st.in_range = false;
    st.expansion = 0.15;
    st.continuation_pressure = 0.4;
    st.structure_quality = 0.7;
    st.volatility = 0.01;

    MarketConceptsEngine concepts;
    PriceDynamics pd;
    const auto rg = concepts.evaluate(pd, st);

    int active = 0;
    for (double s : rg.scores) {
        if (s > 0.0) ++active;
    }
    EXPECT_GT(active, 1);
    // Trend + pullback (+ possibly compression) coexist — not exclusive FSM.
    EXPECT_GT(rg.score(Regime::TrendUp), 0.0);
    EXPECT_GT(rg.score(Regime::PullbackUptrend), 0.0);
    EXPECT_GT(rg.score(Regime::Compression), 0.0);
}

TEST(MarketConcepts, NoHardcodedTriggerThresholdsGateConcepts) {
    // Continuous structure change → continuous score change (no cliff at 0.25/0.5).
    MarketConceptsEngine concepts;
    PriceDynamics pd;

    StructureFeatures weak;
    weak.trend_direction = TrendBias::Up;
    weak.trend_strength = 0.10;
    weak.swing_state = 0.2;
    weak.structure_quality = 0.5;

    StructureFeatures mid = weak;
    mid.trend_strength = 0.40;
    mid.swing_state = 0.6;

    StructureFeatures strong = weak;
    strong.trend_strength = 0.90;
    strong.swing_state = 1.0;

    const double s_weak = concepts.evaluate(pd, weak).score(Regime::TrendUp);
    const double s_mid = concepts.evaluate(pd, mid).score(Regime::TrendUp);
    const double s_strong = concepts.evaluate(pd, strong).score(Regime::TrendUp);

    EXPECT_GT(s_weak, 0.0);
    EXPECT_GT(s_mid, s_weak);
    EXPECT_GT(s_strong, s_mid);

    // Injected Stage-8-ready weights still produce multi-scores (not a trigger gate).
    auto cfg = ConceptWeightConfig::defaults();
    cfg.feature_scales[3] = 1.5;  // amplify trend_strength dim
    MarketConceptsEngine calibrated(cfg);
    const auto rg = calibrated.evaluate(pd, strong);
    EXPECT_EQ(rg.scores.size(), kConceptCount);
    EXPECT_GT(rg.score(Regime::TrendUp), 0.0);
}

TEST(MarketConcepts, SimilarityUsesNormalizedStructureNotExclusiveState) {
    MarketConceptsEngine concepts;
    PriceDynamics pd;

    StructureFeatures a;
    a.trend_direction = TrendBias::Up;
    a.trend_strength = 0.7;
    a.swing_state = 1.0;
    a.breakout_up = true;
    a.breakout_strength = 0.6;
    a.expansion = 0.5;

    StructureFeatures b = a;
    b.failed_breakout_up = true;
    b.failed_breakout = 0.8;
    b.breakout_up = false;
    b.reversal_candidate = 0.6;
    b.structural_invalidation = 0.4;

    const auto ra = concepts.evaluate(pd, a);
    const auto rb = concepts.evaluate(pd, b);

    // Both snapshots expose the full concept vector.
    EXPECT_EQ(ra.scores.size(), 14u);
    EXPECT_EQ(rb.scores.size(), 14u);
    EXPECT_GT(ra.score(Regime::BreakoutUp), 0.0);
    EXPECT_GT(rb.score(Regime::FailedBreakoutUp), 0.0);
    // Prior dominant does not lock / zero other concepts on the next evaluate.
    EXPECT_GT(rb.score(Regime::TrendUp), 0.0);
    EXPECT_GT(rb.score(Regime::ReversalCandidate), 0.0);

    RegimeSimilarity sim;
    // Distinct structure → score vectors are not identical.
    EXPECT_LT(sim.compare(ra, rb), 1.0);
}
