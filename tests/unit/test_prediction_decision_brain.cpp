#include <gtest/gtest.h>

#include "mr/prediction_engine/prediction_engine.hpp"
#include "mr/decision/decision_engine.hpp"
#include "mr/structure_engine/structure_features.hpp"
#include "mr/microstructure_engine/microstructure_features.hpp"
#include "mr/market_concepts/regime_features.hpp"
#include "mr/brain/brain_state.hpp"
#include "mr/market_types/quote.hpp"

using namespace mr;

namespace {

StructureFeatures bullish_structure() {
    StructureFeatures st;
    st.trend_direction = TrendBias::Up;
    st.trend_strength = 0.85;
    st.swing_state = 1.0;
    st.continuation_pressure = 0.8;
    st.pullback_depth = 0.25;
    st.breakout_strength = 0.2;
    st.expansion = 0.4;
    st.compression = 0.1;
    st.structure_quality = 0.8;
    st.structural_invalidation = 0.05;
    st.reversal_candidate = 0.05;
    st.failed_breakout = 0.0;
    st.volatility = 0.2;
    st.bar_count = 40;
    st.swing_count = 6;
    return st;
}

StructureFeatures bearish_structure() {
    auto st = bullish_structure();
    st.trend_direction = TrendBias::Down;
    st.swing_state = -1.0;
    return st;
}

StructureFeatures conflicting_structure() {
    StructureFeatures st;
    st.trend_direction = TrendBias::Range;
    st.trend_strength = 0.2;
    st.swing_state = 0.0;
    st.continuation_pressure = 0.4;
    st.reversal_candidate = 0.4;
    st.failed_breakout = 0.35;
    st.failed_breakout_up = true;
    st.breakout_strength = 0.35;
    st.breakout_up = true;
    st.structure_quality = 0.4;
    st.structural_invalidation = 0.4;
    st.volatility = 0.5;
    st.bar_count = 30;
    st.swing_count = 5;
    return st;
}

MicrostructureFeatures bullish_micro() {
    MicrostructureFeatures m;
    m.has_authority = true;
    m.setup_confirmed = true;
    m.closed_10s_count = 12;
    m.momentum = 0.002;
    m.acceleration = 0.4;
    m.continuation = 0.75;
    m.exhaustion = 0.1;
    m.buyer_pressure = 0.7;
    m.seller_pressure = 0.3;
    m.pressure_delta = 0.4;
    m.acceptance = 0.6;
    m.rejection = 0.15;
    m.reclaim = 0.2;
    m.entry_timing_quality = 0.7;
    m.candle_strength = 0.65;
    m.body_pct = 0.004;
    m.breakout_strength = 0.1;
    m.expansion = 0.3;
    m.volatility = 0.15;
    return m;
}

MicrostructureFeatures bearish_micro() {
    auto m = bullish_micro();
    m.momentum = -0.002;
    m.buyer_pressure = 0.3;
    m.seller_pressure = 0.7;
    m.pressure_delta = -0.4;
    m.rejection = 0.55;
    m.acceptance = 0.2;
    return m;
}

MicrostructureFeatures weak_micro() {
    MicrostructureFeatures m;
    m.has_authority = true;
    m.setup_confirmed = true;
    m.closed_10s_count = 3;
    m.continuation = 0.15;
    m.exhaustion = 0.15;
    m.buyer_pressure = 0.5;
    m.seller_pressure = 0.5;
    m.entry_timing_quality = 0.15;
    m.candle_strength = 0.2;
    return m;
}

MicrostructureFeatures no_authority_micro() {
    auto m = bullish_micro();
    m.has_authority = false;
    m.setup_confirmed = false;
    m.closed_10s_count = 0;
    return m;
}

Quote valid_quote(InstrumentId inst = 1) {
    Quote q;
    q.instrument = inst;
    q.spread.bid = 1999.5;
    q.spread.ask = 2000.5;
    q.valid = true;
    return q;
}

}  // namespace

TEST(PredictionDecisionBrain, LongDominantContinuation) {
    PredictionEngine pred;
    IdGenerator ids;
    DecisionEngine decision(ids);

    auto dual = pred.evaluate(bullish_structure(), true, bullish_micro(), {});
    EXPECT_TRUE(dual.evidence_sufficient);
    EXPECT_GT(dual.long_side.continuation, dual.short_side.continuation);
    EXPECT_GT(dual.long_side.expected_value, dual.short_side.expected_value);
    EXPECT_GT(dual.long_side.thesis_quality, 0.0);

    auto sides = decision.evaluate(dual, 0.05, 1);
    EXPECT_EQ(sides.final_action, TradeAction::Buy);
    EXPECT_EQ(sides.chosen.direction, Direction::Long);

    auto intent = decision.decide(sides.chosen, valid_quote());
    EXPECT_EQ(intent.decision, EntryDecision::EntryReady);
    EXPECT_EQ(intent.direction, Direction::Long);
}

TEST(PredictionDecisionBrain, ShortDominantContinuation) {
    PredictionEngine pred;
    IdGenerator ids;
    DecisionEngine decision(ids);

    auto dual = pred.evaluate(bearish_structure(), true, bearish_micro(), {});
    EXPECT_TRUE(dual.evidence_sufficient);
    EXPECT_GT(dual.short_side.continuation, dual.long_side.continuation);
    EXPECT_GT(dual.short_side.expected_value, dual.long_side.expected_value);

    auto sides = decision.evaluate(dual, 0.05, 1);
    EXPECT_EQ(sides.final_action, TradeAction::Sell);
    EXPECT_EQ(sides.chosen.direction, Direction::Short);

    auto intent = decision.decide(sides.chosen, valid_quote());
    EXPECT_EQ(intent.decision, EntryDecision::EntryReady);
    EXPECT_EQ(intent.direction, Direction::Short);
}

TEST(PredictionDecisionBrain, ConflictingEvidenceWaits) {
    PredictionEngine pred;
    IdGenerator ids;
    DecisionEngine decision(ids);

    auto dual = pred.evaluate(conflicting_structure(), true, weak_micro(), {});
    auto sides = decision.evaluate(dual, 0.05, 1);
    EXPECT_EQ(sides.final_action, TradeAction::Wait);
}

TEST(PredictionDecisionBrain, WeakEvidenceWaits) {
    PredictionEngine pred;
    IdGenerator ids;
    DecisionEngine decision(ids);

    StructureFeatures flat;
    flat.bar_count = 5;
    flat.swing_count = 1;
    flat.structure_quality = 0.1;
    flat.trend_direction = TrendBias::Range;

    auto dual = pred.evaluate(flat, true, weak_micro(), {});
    auto sides = decision.evaluate(dual, 0.2, 1);
    EXPECT_EQ(sides.final_action, TradeAction::Wait);

    auto intent = decision.decide(sides.chosen, valid_quote());
    EXPECT_EQ(intent.decision, EntryDecision::NoTrade);
}

TEST(PredictionDecisionBrain, ReversalFailureEvidence) {
    PredictionEngine pred;
    StructureFeatures st = bullish_structure();
    st.failed_breakout = 0.8;
    st.failed_breakout_up = true;
    st.reversal_candidate = 0.7;
    st.structural_invalidation = 0.6;
    st.continuation_pressure = 0.2;

    MicrostructureFeatures m = bullish_micro();
    m.exhaustion = 0.8;
    m.rejection = 0.7;
    m.failed_breakout = 0.7;
    m.failed_breakout_up = true;
    m.continuation = 0.15;
    m.momentum = -0.001;

    auto dual = pred.evaluate(st, true, m, {});
    EXPECT_GT(dual.long_side.reversal_failure, dual.long_side.continuation);
}

TEST(PredictionDecisionBrain, LongShortAreIndependent) {
    PredictionEngine pred;
    auto dual = pred.evaluate(bullish_structure(), true, bullish_micro(), {});
    EXPECT_GE(dual.long_side.probability, 0.0);
    EXPECT_LE(dual.long_side.probability, 1.0);
    EXPECT_GE(dual.short_side.probability, 0.0);
    EXPECT_LE(dual.short_side.probability, 1.0);
    EXPECT_EQ(dual.long_side.direction, Direction::Long);
    EXPECT_EQ(dual.short_side.direction, Direction::Short);
}

TEST(PredictionDecisionBrain, StructurePlusTenSecondTiming) {
    PredictionEngine pred;
    IdGenerator ids;
    DecisionEngine decision(ids);

    auto dual_no_micro = pred.evaluate(bullish_structure(), true, no_authority_micro(), {});
    EXPECT_FALSE(dual_no_micro.evidence_sufficient);
    auto sides = decision.evaluate(dual_no_micro, 0.05, 1);
    EXPECT_EQ(sides.final_action, TradeAction::Wait);

    auto dual = pred.evaluate(bullish_structure(), true, bullish_micro(), {});
    EXPECT_TRUE(dual.has_structure_authority);
    EXPECT_TRUE(dual.has_micro_authority);
    EXPECT_TRUE(dual.evidence_sufficient);
    sides = decision.evaluate(dual, 0.05, 1);
    EXPECT_EQ(sides.final_action, TradeAction::Buy);
}

TEST(PredictionDecisionBrain, NoStructureAuthorityWaits) {
    PredictionEngine pred;
    IdGenerator ids;
    DecisionEngine decision(ids);

    auto dual = pred.evaluate(bullish_structure(), false, bullish_micro(), {});
    EXPECT_FALSE(dual.evidence_sufficient);
    auto sides = decision.evaluate(dual, 0.05, 1);
    EXPECT_EQ(sides.final_action, TradeAction::Wait);
}

TEST(PredictionDecisionBrain, RawQuoteIsExecutionSafetyOnly) {
    IdGenerator ids;
    DecisionEngine decision(ids);
    PredictionEngine pred;
    auto dual = pred.evaluate(bullish_structure(), true, bullish_micro(), {});
    auto sides = decision.evaluate(dual, 0.05, 1);
    ASSERT_EQ(sides.final_action, TradeAction::Buy);

    Quote bad;
    bad.valid = false;
    auto rejected = decision.decide(sides.chosen, bad);
    EXPECT_EQ(rejected.decision, EntryDecision::Reject);

    auto ok = decision.decide(sides.chosen, valid_quote());
    EXPECT_EQ(ok.decision, EntryDecision::EntryReady);
    EXPECT_GT(dual.long_side.expected_value, dual.short_side.expected_value);
}

TEST(PredictionDecisionBrain, BrainStateHoldsPredictionAndDecision) {
    PredictionEngine pred;
    IdGenerator ids;
    DecisionEngine decision(ids);
    BrainState state;

    StructureFeatures st = bullish_structure();
    MicrostructureFeatures micro = bullish_micro();
    RegimeFeatures rg;
    state.apply_authority(1, st, rg, Timestamp(100));
    state.apply_micro(1, micro, Timestamp(200));

    auto dual = pred.evaluate(st, true, micro, {});
    state.apply_prediction(1, dual, Timestamp(300));
    auto sides = decision.evaluate(dual, 0.05, 1);
    state.apply_decision(1, sides.chosen, sides.final_action, Timestamp(400));

    const auto& ctx = state.latest().instruments.at(1);
    EXPECT_TRUE(ctx.has_structure_authority);
    EXPECT_TRUE(ctx.has_micro_authority);
    EXPECT_TRUE(ctx.has_prediction);
    EXPECT_TRUE(ctx.has_decision);
    EXPECT_EQ(ctx.decision_action, TradeAction::Buy);
    EXPECT_EQ(ctx.structure.trend_direction, TrendBias::Up);
    EXPECT_TRUE(ctx.micro.setup_confirmed);

    BrainContext q;
    q.instrument = 1;
    q.ts = Timestamp(500);
    state.update(q);
    const auto& ctx2 = state.latest().instruments.at(1);
    EXPECT_TRUE(ctx2.has_structure_authority);
    EXPECT_TRUE(ctx2.has_micro_authority);
    EXPECT_TRUE(ctx2.has_prediction);
    EXPECT_TRUE(ctx2.has_decision);
    EXPECT_EQ(ctx2.decision_action, TradeAction::Buy);
}

TEST(PredictionDecisionBrain, WeightConfigIsStage8Calibratable) {
    auto hi = PredictionWeightConfig::defaults();
    hi.w_continuation = 2.0;
    hi.w_trend = 2.0;
    auto lo = PredictionWeightConfig::defaults();
    lo.w_continuation = 0.25;
    lo.w_trend = 0.25;

    PredictionEngine p_hi(hi);
    PredictionEngine p_lo(lo);
    auto d_hi = p_hi.evaluate(bullish_structure(), true, bullish_micro(), {});
    auto d_lo = p_lo.evaluate(bullish_structure(), true, bullish_micro(), {});
    EXPECT_NE(d_hi.long_side.continuation, d_lo.long_side.continuation);

    auto dcfg_hi = DecisionWeightConfig::defaults();
    dcfg_hi.edge_scale = 0.25;
    auto dcfg_lo = DecisionWeightConfig::defaults();
    dcfg_lo.edge_scale = 4.0;
    IdGenerator ids;
    DecisionEngine d1(ids, dcfg_hi);
    DecisionEngine d2(ids, dcfg_lo);
    auto s1 = d1.evaluate(d_hi, 0.05, 1);
    auto s2 = d2.evaluate(d_hi, 0.05, 1);
    EXPECT_TRUE(s1.buy_score != s2.buy_score || s1.wait_score != s2.wait_score);
}

TEST(PredictionDecisionBrain, StopTargetFromStructureVolatilityAndInvalidation) {
    PredictionEngine pred;
    IdGenerator ids;
    DecisionEngine decision(ids);

    auto dual = pred.evaluate(bullish_structure(), true, bullish_micro(), {});
    auto sides = decision.evaluate(dual, 0.05, 1);
    ASSERT_EQ(sides.final_action, TradeAction::Buy);
    EXPECT_GT(sides.chosen.stop_distance_frac, 0.0);
    EXPECT_GT(sides.chosen.target_distance_frac, 0.0);

    // Not the old fixed 0.2% / 0.4% geometry.
    EXPECT_NE(sides.chosen.stop_distance_frac, 0.002);
    EXPECT_NE(sides.chosen.target_distance_frac, 0.004);

    auto intent = decision.decide(sides.chosen, valid_quote());
    ASSERT_EQ(intent.decision, EntryDecision::EntryReady);
    const double mid = intent.reference_price;
    ASSERT_GT(mid, 0.0);
    EXPECT_NEAR(intent.stop_loss, mid * (1.0 - sides.chosen.stop_distance_frac), 1e-9);
    EXPECT_NEAR(intent.take_profit, mid * (1.0 + sides.chosen.target_distance_frac), 1e-9);

    // Authoritative structure context is carried for geometry.
    EXPECT_GE(dual.structure_volatility, 0.0);
    EXPECT_GE(dual.structure_invalidation, 0.0);
    auto st = bullish_structure();
    st.structural_invalidation = 0.8;
    st.volatility = 0.8;
    auto dual_hi = pred.evaluate(st, true, bullish_micro(), {});
    EXPECT_GT(dual_hi.structure_volatility, dual.structure_volatility);
    EXPECT_GT(dual_hi.structure_invalidation, dual.structure_invalidation);
    // Same dual with higher stop_vol / stop_invalidation weights widens stop (Stage-8).
    auto cfg_hi = DecisionWeightConfig::defaults();
    cfg_hi.stop_vol_weight = 3.0;
    cfg_hi.stop_invalidation_weight = 3.0;
    DecisionEngine decision_hi(ids, cfg_hi);
    auto sides_hi = decision_hi.evaluate(dual_hi, 0.05, 1);
    if (sides_hi.final_action == TradeAction::Buy) {
        EXPECT_GE(sides_hi.chosen.stop_distance_frac, sides.chosen.stop_distance_frac);
    }

    // Stage-8 geometry scales are calibratable.
    auto cfg = DecisionWeightConfig::defaults();
    cfg.stop_move_frac = 2.0;
    DecisionEngine decision2(ids, cfg);
    auto sides2 = decision2.evaluate(dual, 0.05, 1);
    ASSERT_EQ(sides2.final_action, TradeAction::Buy);
    EXPECT_NEAR(sides2.chosen.stop_distance_frac, 2.0 * sides.chosen.stop_distance_frac, 1e-9);
}

TEST(PredictionDecisionBrain, PredictionIsNotAnOrder) {
    PredictionEngine pred;
    auto dual = pred.evaluate(bullish_structure(), true, bullish_micro(), {});
    EXPECT_EQ(dual.long_side.direction, Direction::Long);
    EXPECT_EQ(dual.short_side.direction, Direction::Short);
}
