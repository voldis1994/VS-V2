#include "mr/market_core/candidate_replay_eval.hpp"
#include "mr/replay/paper_order_gateway.hpp"

namespace mr {

CandidateReplayMetrics evaluate_candidate_replay(const std::vector<TradeEpisode>& episodes,
                                                 const WeightBundle& weights,
                                                 double account_equity) {
    CandidateReplayMetrics out;
    if (episodes.empty()) return out;

    double sum_pnl = 0.0;
    std::size_t wins = 0;
    for (const auto& ep : episodes) {
        PaperOrderGateway paper;
        EpisodeReplay replay;
        replay.bind_paper_gateway(paper);
        replay.set_account_equity(account_equity);
        replay.set_weight_bundle(weights);
        const auto result = replay.run(ep);

        // Candidate result = real replay PnL / trade outcomes only.
        const double pnl = result.total_pnl;
        sum_pnl += pnl;
        if (pnl > 0.0) ++wins;
        out.market_events += result.raw_count + result.closed_10s_count + result.authority_count;
        out.decisions += result.decisions;
        out.entry_ready += result.entry_ready;
        out.entries += result.entries;
        out.exits += result.exits;
        ++out.n;
    }
    out.mean_pnl = sum_pnl / static_cast<double>(out.n);
    out.edge = out.mean_pnl;
    out.win_rate = static_cast<double>(wins) / static_cast<double>(out.n);
    out.used_production_replay = true;
    return out;
}

}  // namespace mr
