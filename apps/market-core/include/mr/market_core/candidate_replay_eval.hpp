#pragma once

#include "mr/market_core/episode_replay.hpp"
#include "mr/market_core/weight_bundle.hpp"
#include "mr/memory_engine/episode_types.hpp"

#include <cstddef>
#include <vector>

namespace mr {

struct CandidateReplayMetrics {
    double edge{0.0};
    double mean_pnl{0.0};
    double win_rate{0.0};
    std::size_t n{0};
    std::size_t market_events{0};
    bool used_production_replay{true};
};

/**
 * Evaluate candidate weights on historical episodes via Stage-7 production
 * EpisodeReplay + Brain pipeline. Ground truth is production replay outcome —
 * not a Python proxy_pnl formula.
 */
CandidateReplayMetrics evaluate_candidate_replay(
    const std::vector<TradeEpisode>& episodes,
    const WeightBundle& weights,
    double account_equity = 50'000.0);

double production_replay_episode_edge(const TradeEpisode& episode,
                                      const WeightBundle& weights,
                                      const EpisodeReplay::Result& replay_result);

}  // namespace mr
