#include <gtest/gtest.h>

#include "mr/market_core/candidate_replay_eval.hpp"
#include "mr/market_core/episode_replay.hpp"
#include "mr/market_core/weight_bundle.hpp"
#include "mr/replay/paper_order_gateway.hpp"

#include <cmath>

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

/** Trending multi-clock episode — enough authority bars for Brain decisions. */
TradeEpisode build_trend_episode() {
    TradeEpisode ep;
    ep.episode_id = "stage8-trend-1";
    ep.instrument = 1;
    ep.sealed = true;
    // Huge recorded outcome — must NOT become candidate result.
    ep.outcome.sealed = true;
    ep.outcome.direction = Direction::Long;
    ep.outcome.realized_pnl = 999'999.0;
    ep.outcome.mfe = 1'000'000.0;
    ep.outcome.mae = 1.0;
    ep.outcome.quantity = 1.0;
    ep.outcome.entry_price = 2000.0;
    ep.outcome.exit_price = 3000.0;

    std::uint64_t seq = 1;
    double px = 2000.0;
    for (int i = 0; i < 40; ++i) {
        const std::int64_t t0 = (i + 1) * 60'000'000'000LL;
        px += 0.8 + (i % 3) * 0.15;

        EpisodeMarketItem raw;
        raw.domain = EpisodeClockDomain::RawQuote;
        raw.ts = Timestamp{t0};
        raw.sequence = seq++;
        raw.instrument = 1;
        raw.quote = make_quote(1, t0, px, seq);
        ep.market.push_back(raw);

        EpisodeMarketItem closed10;
        closed10.domain = EpisodeClockDomain::ClosedTenSecond;
        closed10.ts = Timestamp{t0 + 10'000'000'000LL};
        closed10.sequence = seq++;
        closed10.instrument = 1;
        closed10.one_shot = true;
        closed10.candle = make_candle(1, t0, px - 0.4, px + 0.5, px - 0.6, px + 0.2);
        ep.market.push_back(closed10);

        EpisodeMarketItem auth;
        auth.domain = EpisodeClockDomain::AuthorityOhlc;
        auth.ts = Timestamp{t0 + 50'000'000'000LL};
        auth.sequence = seq++;
        auth.instrument = 1;
        auth.structure_authority = true;
        auth.timeframe = Timeframe::Minute1;
        auth.candle = make_candle(1, t0, px - 0.5, px + 1.2, px - 0.8, px + 0.9);
        ep.market.push_back(auth);
    }
    return ep;
}

EpisodeReplay::Result run_with_weights(const TradeEpisode& ep, const WeightBundle& w) {
    PaperOrderGateway paper;
    EpisodeReplay replay;
    replay.bind_paper_gateway(paper);
    replay.set_account_equity(50'000.0);
    replay.set_weight_bundle(w);
    return replay.run(ep);
}

bool outcomes_differ(const EpisodeReplay::Result& a, const EpisodeReplay::Result& b) {
    if (a.decisions != b.decisions) return true;
    if (a.entry_ready != b.entry_ready) return true;
    if (a.entries != b.entries) return true;
    if (a.exits != b.exits) return true;
    if (a.open_positions.size() != b.open_positions.size()) return true;
    if (a.pending_intents.size() != b.pending_intents.size()) return true;
    if (std::abs(a.total_pnl - b.total_pnl) > 1e-9) return true;
    if (std::abs(a.realized_pnl - b.realized_pnl) > 1e-9) return true;
    if (std::abs(a.unrealized_pnl - b.unrealized_pnl) > 1e-9) return true;
    return false;
}

}  // namespace

TEST(Stage8CandidateReplay, IgnoresRecordedOutcomeForCandidateResult) {
    auto ep = build_trend_episode();
    ASSERT_DOUBLE_EQ(ep.outcome.realized_pnl, 999'999.0);

    WeightBundle w = WeightBundle::defaults();
    const auto metrics = evaluate_candidate_replay({ep}, w, 50'000.0);

    // Candidate metrics must come from replay Result, not recorded outcome.
    EXPECT_LT(std::abs(metrics.mean_pnl), 100'000.0);
    EXPECT_NE(metrics.mean_pnl, ep.outcome.realized_pnl);
    EXPECT_TRUE(metrics.used_production_replay);
}

TEST(Stage8CandidateReplay, DifferentWeightsProduceDifferentReplayOutcomes) {
    const auto ep = build_trend_episode();

    WeightBundle aggressive = WeightBundle::defaults();
    aggressive.decision.edge_scale = 0.01;  // saturates soft01 quickly → more action
    aggressive.prediction.probability_scale = 2.0;
    aggressive.prediction.continuation_scale = 2.0;
    aggressive.risk_spread_cost_scale = 0.1;
    aggressive.risk_min_net_ev_scale = 0.1;

    WeightBundle conservative = WeightBundle::defaults();
    conservative.decision.edge_scale = 50.0;  // soft01 stays tiny → wait dominates
    conservative.prediction.probability_scale = 0.1;
    conservative.prediction.continuation_scale = 0.1;
    conservative.risk_spread_cost_scale = 10.0;
    conservative.risk_min_net_ev_scale = 10.0;

    const auto ra = run_with_weights(ep, aggressive);
    const auto rb = run_with_weights(ep, conservative);

    EXPECT_TRUE(outcomes_differ(ra, rb))
        << "aggressive decisions=" << ra.decisions << " entry_ready=" << ra.entry_ready
        << " entries=" << ra.entries << " exits=" << ra.exits << " pnl=" << ra.total_pnl
        << " | conservative decisions=" << rb.decisions << " entry_ready=" << rb.entry_ready
        << " entries=" << rb.entries << " exits=" << rb.exits << " pnl=" << rb.total_pnl;

    const auto ma = evaluate_candidate_replay({ep}, aggressive, 50'000.0);
    const auto mb = evaluate_candidate_replay({ep}, conservative, 50'000.0);

    // Promotion compares these replay outcomes directly.
    const bool metrics_differ =
        std::abs(ma.edge - mb.edge) > 1e-12 || ma.decisions != mb.decisions
        || ma.entry_ready != mb.entry_ready || ma.entries != mb.entries || ma.exits != mb.exits;
    EXPECT_TRUE(metrics_differ);
    EXPECT_DOUBLE_EQ(ma.mean_pnl, ra.total_pnl);
    EXPECT_DOUBLE_EQ(mb.mean_pnl, rb.total_pnl);
}

TEST(Stage8CandidateReplay, ResultUsesReplayPnLNotManualScore) {
    const auto ep = build_trend_episode();
    WeightBundle w = WeightBundle::defaults();
    w.decision.edge_scale = 0.05;

    PaperOrderGateway paper;
    EpisodeReplay replay;
    replay.bind_paper_gateway(paper);
    replay.set_account_equity(50'000.0);
    replay.set_weight_bundle(w);
    const auto result = replay.run(ep);
    const auto metrics = evaluate_candidate_replay({ep}, w, 50'000.0);

    EXPECT_DOUBLE_EQ(metrics.mean_pnl, result.total_pnl);
    EXPECT_EQ(metrics.entries, result.entries);
    EXPECT_EQ(metrics.exits, result.exits);
    EXPECT_EQ(metrics.decisions, result.decisions);
}
