#include "mr/market_core/candidate_replay_eval.hpp"
#include "mr/replay/paper_order_gateway.hpp"

#include <algorithm>
#include <cmath>

namespace mr {
namespace {

double clamp01(double x) { return std::max(0.0, std::min(1.0, x)); }

const EpisodeBrainFrame* entry_frame(const TradeEpisode& ep) {
    for (const auto& f : ep.frames) {
        if (f.has_decision && f.has_prediction) return &f;
    }
    return ep.frames.empty() ? nullptr : &ep.frames.front();
}

/**
 * Production-side edge after EpisodeReplay.
 * Distinct from Python training proxy_pnl blend.
 */
double score_after_replay(const TradeEpisode& ep, const WeightBundle& w) {
    const auto* frame = entry_frame(ep);
    double pred_p = 0.5;
    double pred_c = 0.5;
    double pred_r = 0.5;
    double exit_q = 0.5;
    if (frame != nullptr) {
        const auto& side = (ep.outcome.direction == Direction::Short)
                               ? frame->prediction.short_side
                               : frame->prediction.long_side;
        pred_p = side.probability;
        pred_c = side.continuation;
        pred_r = side.reversal_failure;
    }
    if (ep.outcome.mfe > 1e-12) {
        exit_q = clamp01(ep.outcome.realized_pnl / ep.outcome.mfe);
    }

    const double cal_p = clamp01(pred_p * w.prediction.probability_scale);
    const double cal_c = clamp01(pred_c * w.prediction.continuation_scale);
    const double cal_r = clamp01(pred_r * w.prediction.reversal_scale);
    const double win = ep.outcome.realized_pnl > 0.0 ? 1.0 : 0.0;
    const double align = 1.0 - std::abs(cal_p - win);
    const double cont_target = ep.outcome.mfe / (ep.outcome.mfe + ep.outcome.mae + 1e-12);
    const double cont_term = 1.0 - std::abs(cal_c - cont_target);
    const double rev_penalty = cal_r * (1.0 - win);
    const double exit_term = clamp01(exit_q * w.position.exit_scale);

    double edge = ep.outcome.realized_pnl
                  * (0.35 + 0.35 * align + 0.15 * cont_term + 0.15 * exit_term - 0.20 * rev_penalty);
    edge *= (0.55 + 0.45 * std::max(0.1, w.decision.edge_scale));
    if (w.position.exit_scale > 1.0) {
        edge *= 1.0 / w.position.exit_scale;
    }
    return edge;
}

}  // namespace

double production_replay_episode_edge(const TradeEpisode& episode,
                                      const WeightBundle& weights,
                                      const EpisodeReplay::Result& /*replay_result*/) {
    return score_after_replay(episode, weights);
}

CandidateReplayMetrics evaluate_candidate_replay(const std::vector<TradeEpisode>& episodes,
                                                 const WeightBundle& weights,
                                                 double account_equity) {
    CandidateReplayMetrics out;
    if (episodes.empty()) return out;

    double sum = 0.0;
    std::size_t wins = 0;
    for (const auto& ep : episodes) {
        PaperOrderGateway paper;
        EpisodeReplay replay;
        replay.bind_paper_gateway(paper);
        replay.set_account_equity(account_equity);
        replay.set_weight_bundle(weights);
        const auto result = replay.run(ep);

        const double edge = production_replay_episode_edge(ep, weights, result);
        sum += edge;
        if (edge > 0.0) ++wins;
        out.market_events += result.raw_count + result.closed_10s_count + result.authority_count;
        ++out.n;
    }
    out.mean_pnl = sum / static_cast<double>(out.n);
    out.edge = out.mean_pnl;
    out.win_rate = static_cast<double>(wins) / static_cast<double>(out.n);
    out.used_production_replay = true;
    return out;
}

}  // namespace mr
