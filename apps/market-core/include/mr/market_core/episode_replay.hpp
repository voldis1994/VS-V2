#pragma once

#include "mr/memory_engine/episode_types.hpp"
#include "mr/market_core/pipeline.hpp"
#include "mr/market_core/weight_bundle.hpp"
#include "mr/microstructure_engine/microstructure_features.hpp"
#include "mr/replay/paper_order_gateway.hpp"

#include <optional>
#include <vector>

namespace mr {

/**
 * Replays a TradeEpisode market stream into the production MarketCorePipeline
 * in correct multi-clock order:
 *   RAW → process_event (quote path)
 *   CLOSED 10s → process_closed_10s (one-shot micro authority)
 *   1m+ → process_authority_ohlc (structure authority)
 * Never binds Capital LIVE — paper only.
 *
 * Result metrics (decisions/entries/exits/PnL) come from the live replay run —
 * not from the recorded episode outcome.
 */
class EpisodeReplay {
public:
    struct Result {
        std::vector<EpisodeClockDomain> clock_order;
        BrainSnapshot final_brain{};
        MicrostructureFeatures final_micro{};
        std::vector<PositionState> open_positions;
        std::vector<TradeIntent> pending_intents;
        std::size_t raw_count{0};
        std::size_t closed_10s_count{0};
        std::size_t authority_count{0};
        bool used_live_gateway{false};

        /** Production-path counters from this replay (not recorded outcome). */
        std::size_t decisions{0};
        std::size_t entry_ready{0};
        std::size_t entries{0};
        std::size_t exits{0};
        double realized_pnl{0.0};
        double unrealized_pnl{0.0};
        double total_pnl{0.0};
    };

    EpisodeReplay() = default;

    /** Optional paper gateway — if unset, no execution (pending intents only). */
    void bind_paper_gateway(PaperOrderGateway& gateway);

    void set_account_equity(double equity);

    /** Stage-8 candidate weights applied to production Brain before replay. */
    void set_weight_bundle(const WeightBundle& bundle);

    Result run(const TradeEpisode& episode);

private:
    PaperOrderGateway* paper_{nullptr};
    double equity_{0};
    bool equity_set_{false};
    std::optional<WeightBundle> weights_{};
};

}  // namespace mr
