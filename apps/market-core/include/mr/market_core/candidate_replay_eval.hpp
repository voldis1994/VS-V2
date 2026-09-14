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
    std::size_t decisions{0};
    std::size_t entry_ready{0};
    std::size_t entries{0};
    std::size_t exits{0};
    bool used_production_replay{true};
};

/**
 * Evaluate candidate weights via Stage-7 production EpisodeReplay + Brain.
 * Metrics are aggregated from EpisodeReplay::Result (decisions/trades/PnL
 * produced by the replay). Recorded episode.outcome is NOT used as the
 * candidate result (it may only serve as an external benchmark elsewhere).
 */
CandidateReplayMetrics evaluate_candidate_replay(
    const std::vector<TradeEpisode>& episodes,
    const WeightBundle& weights,
    double account_equity = 50'000.0);

}  // namespace mr
