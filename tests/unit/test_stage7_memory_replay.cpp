#include <gtest/gtest.h>

#include "mr/market_core/episode_replay.hpp"
#include "mr/market_core/pipeline.hpp"
#include "mr/memory_engine/episode_recorder.hpp"
#include "mr/memory_engine/episode_store.hpp"
#include "mr/replay/paper_order_gateway.hpp"
#include "mr/decision/trade_decision.hpp"
#include "mr/prediction_engine/prediction.hpp"
#include "mr/perception_engine/price_dynamics.hpp"

#include <filesystem>

using namespace mr;

namespace {

MarketEvent make_quote(InstrumentId inst, std::int64_t ts_ns, double mid, std::uint64_t seq) {
    MarketEvent e;
    e.instrument = inst;
    e.source = 1;
    e.type = MarketEventType::Quote;
    e.receive_timestamp = Timestamp{ts_ns};
    e.exchange_timestamp = Timestamp{ts_ns};
    e.provider_timestamp = Timestamp{ts_ns};
    e.bid = mid - 0.05;
    e.ask = mid + 0.05;
    e.last = mid;
    e.sequence = seq;
    return e;
}

Candle make_candle(InstrumentId inst, std::int64_t open_ns, double o, double h, double l,
                   double c) {
    Candle candle;
    candle.instrument = inst;
    candle.open_time = Timestamp{open_ns};
    candle.open = o;
    candle.high = h;
    candle.low = l;
    candle.close = c;
    candle.ticks = 8;
    candle.status = CandleStatus::Closed;
    return candle;
}

TradeIntent ready_long() {
    TradeIntent intent;
    intent.id = 7;
    intent.instrument = 1;
    intent.direction = Direction::Long;
    intent.created_at = Timestamp{1'000'000'000};
    intent.reference_price = 2000.0;
    intent.probability = 0.8;
    intent.expected_value = 2.0;
    intent.stop_loss = 1990.0;
    intent.take_profit = 2040.0;
    intent.decision = EntryDecision::EntryReady;
    return intent;
}

DualPrediction strong_long() {
    DualPrediction d;
    d.has_structure_authority = true;
    d.has_micro_authority = true;
    d.evidence_sufficient = true;
    d.long_side.direction = Direction::Long;
    d.long_side.continuation = 0.9;
    d.long_side.reversal_failure = 0.05;
    d.long_side.expected_move = 1.0;
    d.long_side.adverse_move = 0.2;
    d.long_side.probability = 0.8;
    d.long_side.confidence = 0.85;
    d.long_side.expected_value = 1.5;
    d.long_side.invalidation = 0.05;
    d.long_side.thesis_quality = 0.9;
    d.short_side.direction = Direction::Short;
    d.short_side.continuation = 0.1;
    d.short_side.thesis_quality = 0.1;
    return d;
}

TradeEpisode build_sample_episode() {
    EpisodeRecorder rec;
    rec.set_model_id("stage7-test-model");
    rec.set_config_hash("cfg-abc");
    rec.set_weight_hashes(PredictionWeightConfig::defaults(), DecisionWeightConfig::defaults(),
                          RiskWeightConfig::defaults(), ExecutionWeightConfig::defaults(),
                          PositionWeightConfig::defaults());
    rec.begin_episode("ep-full-1", 1);

    rec.record_raw_quote(make_quote(1, 1'000'000'000, 2000.0, 1));
    rec.record_closed_10s(make_candle(1, 1'000'000'000, 1999.5, 2000.5, 1999.0, 2000.2),
                          Timestamp{1'010'000'000}, true);
    rec.record_raw_quote(make_quote(1, 1'020'000'000, 2001.0, 2));
    rec.record_authority_ohlc(make_candle(1, 1'060'000'000, 2000.0, 2002.0, 1999.0, 2001.5),
                              Timeframe::Minute1, Timestamp{1'060'000'000});

    PositionState pos;
    pos.instrument = 1;
    pos.direction = Direction::Long;
    pos.entry_price = 2000.0;
    pos.quantity = 2.0;
    pos.opened_at = Timestamp{1'060'000'000};
    pos.deal_id = "DEAL-1";
    rec.on_entry(pos, Timestamp{1'060'000'000});
    rec.on_mark(2010.0, Timestamp{1'070'000'000});
    rec.on_mark(1995.0, Timestamp{1'080'000'000});

    PositionDecision protect;
    protect.action = PositionAction::Protect;
    protect.reason = ExitReason::PeakProtection;
    protect.suggested_stop = 2005.0;
    protect.reason_codes.push_back("PROTECT");
    rec.record_position_action(protect);

    PositionDecision exitd;
    exitd.action = PositionAction::Exit;
    exitd.reason = ExitReason::ThesisFailure;
    exitd.reason_codes.push_back("EXIT");
    pos.mfe = 10.0;
    pos.mae = 5.0;
    pos.peak_retention = 0.5;
    rec.on_exit(pos, exitd, 2004.0, Timestamp{1'090'000'000});
    rec.on_post_exit_mark(2012.0, Timestamp{1'100'000'000});
    rec.on_post_exit_mark(1990.0, Timestamp{1'110'000'000});
    rec.seal_episode();
    return rec.take_episode();
}

}  // namespace

TEST(Stage7MemoryReplay, FullEpisodeCapturesClocksActionsOutcome) {
    const auto ep = build_sample_episode();
    ASSERT_TRUE(ep.sealed);
    ASSERT_EQ(ep.market.size(), 4u);
    EXPECT_EQ(ep.market[0].domain, EpisodeClockDomain::RawQuote);
    EXPECT_EQ(ep.market[1].domain, EpisodeClockDomain::ClosedTenSecond);
    EXPECT_EQ(ep.market[2].domain, EpisodeClockDomain::RawQuote);
    EXPECT_EQ(ep.market[3].domain, EpisodeClockDomain::AuthorityOhlc);
    EXPECT_TRUE(ep.market[3].structure_authority);
    EXPECT_FALSE(ep.market[1].structure_authority);

    EXPECT_EQ(ep.provenance.model_id, "stage7-test-model");
    EXPECT_FALSE(ep.provenance.brain_version.empty());
    EXPECT_FALSE(ep.provenance.prediction_weights_hash.empty());

    ASSERT_GE(ep.position_actions.size(), 2u);
    EXPECT_EQ(ep.position_actions[0].action, PositionAction::Protect);
    EXPECT_EQ(ep.outcome.exit_action, PositionAction::Exit);
    EXPECT_DOUBLE_EQ(ep.outcome.mfe, 10.0);
    EXPECT_DOUBLE_EQ(ep.outcome.mae, 5.0);
    EXPECT_GT(ep.outcome.post_exit_favorable, 0.0);
    EXPECT_GT(ep.outcome.post_exit_adverse, 0.0);
    EXPECT_GE(ep.outcome.post_exit_samples, 2u);
}

TEST(Stage7MemoryReplay, RestartPersistenceRoundTrip) {
    const auto ep = build_sample_episode();
    const auto root = std::filesystem::temp_directory_path() / "vs_v2_stage7_episodes";
    std::filesystem::remove_all(root);
    EpisodeStore store(root);
    store.save(ep);
    ASSERT_TRUE(store.exists(ep.episode_id));

    const auto loaded = store.load(ep.episode_id);
    EXPECT_EQ(loaded.episode_id, ep.episode_id);
    EXPECT_EQ(loaded.market.size(), ep.market.size());
    EXPECT_EQ(loaded.market[1].domain, EpisodeClockDomain::ClosedTenSecond);
    EXPECT_EQ(loaded.provenance.model_id, ep.provenance.model_id);
    EXPECT_EQ(loaded.provenance.prediction_weights_hash, ep.provenance.prediction_weights_hash);
    EXPECT_DOUBLE_EQ(loaded.outcome.mfe, ep.outcome.mfe);
    EXPECT_DOUBLE_EQ(loaded.outcome.mae, ep.outcome.mae);
    EXPECT_DOUBLE_EQ(loaded.outcome.realized_pnl, ep.outcome.realized_pnl);
    EXPECT_EQ(loaded.outcome.post_exit_samples, ep.outcome.post_exit_samples);
    EXPECT_EQ(store.list_ids().size(), 1u);
    std::filesystem::remove_all(root);
}

TEST(Stage7MemoryReplay, ClockOrderingPreserved) {
    const auto ep = build_sample_episode();
    EpisodeReplay replay;
    replay.set_account_equity(50'000.0);
    const auto result = replay.run(ep);
    ASSERT_EQ(result.clock_order.size(), ep.market.size());
    EXPECT_EQ(result.clock_order[0], EpisodeClockDomain::RawQuote);
    EXPECT_EQ(result.clock_order[1], EpisodeClockDomain::ClosedTenSecond);
    EXPECT_EQ(result.clock_order[2], EpisodeClockDomain::RawQuote);
    EXPECT_EQ(result.clock_order[3], EpisodeClockDomain::AuthorityOhlc);
    EXPECT_EQ(result.raw_count, 2u);
    EXPECT_EQ(result.closed_10s_count, 1u);
    EXPECT_EQ(result.authority_count, 1u);
    EXPECT_FALSE(result.used_live_gateway);
}

TEST(Stage7MemoryReplay, ReplayDeterminismSameEpisodeSameResults) {
    TradeEpisode ep;
    ep.episode_id = "det-1";
    ep.instrument = 1;
    ep.provenance.model_id = "det-model";
    ep.provenance.config_hash = "det-cfg";
    for (int i = 0; i < 20; ++i) {
        EpisodeMarketItem item;
        item.domain = EpisodeClockDomain::RawQuote;
        item.ts = Timestamp{(i + 1) * 1'000'000'000ll};
        item.sequence = static_cast<std::uint64_t>(i + 1);
        item.instrument = 1;
        item.quote = make_quote(1, (i + 1) * 1'000'000'000ll, 2000.0 + i * 0.1, i + 1);
        ep.market.push_back(item);
        if (i % 5 == 4) {
            EpisodeMarketItem auth;
            auth.domain = EpisodeClockDomain::AuthorityOhlc;
            auth.ts = Timestamp{(i + 1) * 1'000'000'000ll + 1};
            auth.sequence = static_cast<std::uint64_t>(i + 100);
            auth.instrument = 1;
            auth.structure_authority = true;
            auth.one_shot = true;
            auth.timeframe = Timeframe::Minute1;
            auth.candle = make_candle(1, auth.ts.count(), 2000, 2001, 1999, 2000.5 + i * 0.01);
            ep.market.push_back(auth);
        }
    }
    ep.sealed = true;

    EpisodeReplay a;
    EpisodeReplay b;
    a.set_account_equity(25'000.0);
    b.set_account_equity(25'000.0);
    const auto ra = a.run(ep);
    const auto rb = b.run(ep);
    EXPECT_EQ(ra.clock_order, rb.clock_order);
    EXPECT_EQ(ra.raw_count, rb.raw_count);
    EXPECT_EQ(ra.authority_count, rb.authority_count);
    EXPECT_EQ(ra.pending_intents.size(), rb.pending_intents.size());
    EXPECT_EQ(ra.open_positions.size(), rb.open_positions.size());
}

TEST(Stage7MemoryReplay, NoLiveExecutionOnReplayPath) {
    PaperOrderGateway paper;
    EpisodeReplay replay;
    replay.bind_paper_gateway(paper);
    replay.set_account_equity(50'000.0);

    MarketCorePipeline pipe;
    pipe.set_operating_mode(OperatingMode::Replay);
    EXPECT_EQ(pipe.operating_mode(), OperatingMode::Replay);

    const auto result = replay.run(build_sample_episode());
    EXPECT_FALSE(result.used_live_gateway);
}

TEST(Stage7MemoryReplay, PipelineRecorderCaptureAndOutcomeMetrics) {
    PaperOrderGateway paper;
    EpisodeRecorder recorder;
    recorder.set_model_id("pipe-model");
    recorder.set_weight_hashes(PredictionWeightConfig::defaults(), DecisionWeightConfig::defaults(),
                               RiskWeightConfig::defaults(), ExecutionWeightConfig::defaults(),
                               PositionWeightConfig::defaults());
    recorder.begin_episode("pipe-ep-1", 1);

    MarketCorePipeline pipeline;
    pipeline.set_operating_mode(OperatingMode::Paper);
    pipeline.bind_order_gateway(paper);
    pipeline.set_account_equity(50'000.0);
    pipeline.attach_episode_recorder(&recorder);

    ASSERT_TRUE(pipeline.enter_from_decision(ready_long(), strong_long(), 2000.0, 0.2));
    ASSERT_EQ(pipeline.open_positions().size(), 1u);

    pipeline.update_open_positions(1, strong_long(), {}, 2010.0);
    DualPrediction bad = strong_long();
    bad.long_side.continuation = 0.05;
    bad.long_side.reversal_failure = 0.95;
    bad.long_side.invalidation = 0.95;
    bad.long_side.thesis_quality = 0.05;
    bad.long_side.expected_value = -1.0;
    pipeline.update_open_positions(1, bad, {}, 1998.0);

    pipeline.process_event(make_quote(1, 2'000'000'000, 2015.0, 9));
    pipeline.process_event(make_quote(1, 2'100'000'000, 1988.0, 10));
    recorder.seal_episode();

    const auto ep = recorder.take_episode();
    EXPECT_TRUE(ep.sealed);
    EXPECT_FALSE(ep.provenance.prediction_weights_hash.empty());
    EXPECT_GE(ep.outcome.mfe, 0.0);
    EXPECT_GE(ep.outcome.mae, 0.0);
    EXPECT_GT(ep.outcome.entry_price, 0.0);
}

namespace {

TradeEpisode build_closed10s_episode(bool include_closed_10s, const std::string& id) {
    EpisodeRecorder rec;
    rec.set_model_id("micro-path-model");
    rec.set_config_hash("cfg-micro-path");
    rec.set_weight_hashes(PredictionWeightConfig::defaults(), DecisionWeightConfig::defaults(),
                          RiskWeightConfig::defaults(), ExecutionWeightConfig::defaults(),
                          PositionWeightConfig::defaults());
    rec.begin_episode(id, 1);

    rec.record_raw_quote(make_quote(1, 1'000'000'000, 2000.0, 1));

    // Distinctive CLOSED 10s bar — large bullish body so micro geometry is non-zero.
    const Candle closed10s = make_candle(1, 1'000'000'000, 2000.0, 2012.0, 1999.0, 2011.0);
    if (include_closed_10s) {
        rec.record_closed_10s(closed10s, Timestamp{11'000'000'000}, true);
    }

    // Authority 1m+ bars drive structure → prediction/decision on production path.
    for (int i = 0; i < 6; ++i) {
        const double base = 2000.0 + static_cast<double>(i);
        const std::int64_t open_ns = 60'000'000'000ll * (i + 1);
        rec.record_authority_ohlc(
            make_candle(1, open_ns, base, base + 1.5, base - 0.4, base + 1.0), Timeframe::Minute1,
            Timestamp{open_ns});
    }

    rec.seal_episode();
    return rec.take_episode();
}

void feed_production_closed10s_path(MarketCorePipeline& pipeline, bool include_closed_10s) {
    pipeline.set_operating_mode(OperatingMode::Replay);
    pipeline.set_account_equity(50'000.0);
    pipeline.process_event(make_quote(1, 1'000'000'000, 2000.0, 1));
    if (include_closed_10s) {
        pipeline.process_closed_10s(
            make_candle(1, 1'000'000'000, 2000.0, 2012.0, 1999.0, 2011.0),
            Timestamp{11'000'000'000});
    }
    for (int i = 0; i < 6; ++i) {
        const double base = 2000.0 + static_cast<double>(i);
        const std::int64_t open_ns = 60'000'000'000ll * (i + 1);
        pipeline.process_authority_ohlc(
            make_candle(1, open_ns, base, base + 1.5, base - 0.4, base + 1.0), Timeframe::Minute1);
    }
}

}  // namespace

TEST(Stage7MemoryReplay, RecordedClosed10sDrivesMicroAndMatchesProduction) {
    const auto with_10s = build_closed10s_episode(true, "ep-with-10s");
    const auto without_10s = build_closed10s_episode(false, "ep-no-10s");

    // Replay WITH recorded CLOSED 10s → production micro authority path.
    EpisodeReplay replay;
    replay.set_account_equity(50'000.0);
    const auto replayed = replay.run(with_10s);
    EXPECT_EQ(replayed.closed_10s_count, 1u);
    EXPECT_TRUE(replayed.final_micro.has_authority);
    EXPECT_EQ(replayed.final_micro.closed_10s_count, 1u);
    EXPECT_GT(replayed.final_micro.body_pct, 0.0);
    EXPECT_EQ(replayed.final_micro.authority_tf, Timeframe::Second10);

    // Same episode without CLOSED 10s → micro evidence must differ.
    const auto skipped = replay.run(without_10s);
    EXPECT_EQ(skipped.closed_10s_count, 0u);
    EXPECT_FALSE(skipped.final_micro.has_authority);
    EXPECT_EQ(skipped.final_micro.closed_10s_count, 0u);
    EXPECT_NE(skipped.final_micro.body_pct, replayed.final_micro.body_pct);

    // Direct production process_closed_10s path must match EpisodeReplay reinject.
    MarketCorePipeline production;
    feed_production_closed10s_path(production, true);
    const auto prod_micro = production.micro().snapshot();
    const auto prod_brain = production.brain_snapshot();

    EXPECT_EQ(prod_micro.has_authority, replayed.final_micro.has_authority);
    EXPECT_EQ(prod_micro.closed_10s_count, replayed.final_micro.closed_10s_count);
    EXPECT_DOUBLE_EQ(prod_micro.body_pct, replayed.final_micro.body_pct);
    EXPECT_DOUBLE_EQ(prod_micro.upper_wick_pct, replayed.final_micro.upper_wick_pct);
    EXPECT_DOUBLE_EQ(prod_micro.lower_wick_pct, replayed.final_micro.lower_wick_pct);
    EXPECT_DOUBLE_EQ(prod_micro.continuation, replayed.final_micro.continuation);
    EXPECT_DOUBLE_EQ(prod_micro.entry_timing_quality, replayed.final_micro.entry_timing_quality);

    ASSERT_TRUE(prod_brain.instruments.count(1));
    ASSERT_TRUE(replayed.final_brain.instruments.count(1));
    const auto& pc = prod_brain.instruments.at(1);
    const auto& rc = replayed.final_brain.instruments.at(1);
    EXPECT_TRUE(pc.has_micro_authority);
    EXPECT_TRUE(rc.has_micro_authority);
    EXPECT_EQ(pc.has_structure_authority, rc.has_structure_authority);
    EXPECT_EQ(pc.has_prediction, rc.has_prediction);
    EXPECT_EQ(pc.has_decision, rc.has_decision);
    EXPECT_EQ(pc.decision_action, rc.decision_action);
    EXPECT_DOUBLE_EQ(pc.prediction.long_side.continuation, rc.prediction.long_side.continuation);
    EXPECT_DOUBLE_EQ(pc.prediction.long_side.expected_value, rc.prediction.long_side.expected_value);
    EXPECT_DOUBLE_EQ(pc.prediction.short_side.continuation, rc.prediction.short_side.continuation);
    EXPECT_DOUBLE_EQ(pc.micro.body_pct, rc.micro.body_pct);
    EXPECT_EQ(pc.micro.closed_10s_count, rc.micro.closed_10s_count);

    // Deterministic: two replays of the same recorded stream restore the same brain.
    const auto replayed_b = replay.run(with_10s);
    ASSERT_TRUE(replayed_b.final_brain.instruments.count(1));
    const auto& rb = replayed_b.final_brain.instruments.at(1);
    EXPECT_EQ(rc.has_prediction, rb.has_prediction);
    EXPECT_EQ(rc.has_decision, rb.has_decision);
    EXPECT_EQ(rc.decision_action, rb.decision_action);
    EXPECT_DOUBLE_EQ(rc.prediction.long_side.continuation, rb.prediction.long_side.continuation);
    EXPECT_DOUBLE_EQ(rc.prediction.long_side.expected_value, rb.prediction.long_side.expected_value);
    EXPECT_DOUBLE_EQ(rc.micro.body_pct, rb.micro.body_pct);
    EXPECT_DOUBLE_EQ(replayed.final_micro.continuation, replayed_b.final_micro.continuation);
}
