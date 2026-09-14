#include "mr/market_core/episode_replay.hpp"

namespace mr {

void EpisodeReplay::bind_paper_gateway(PaperOrderGateway& gateway) { paper_ = &gateway; }

void EpisodeReplay::set_account_equity(double equity) {
    equity_ = equity;
    equity_set_ = equity > 0.0;
}

void EpisodeReplay::set_weight_bundle(const WeightBundle& bundle) { weights_ = bundle; }

EpisodeReplay::Result EpisodeReplay::run(const TradeEpisode& episode) {
    Result out;
    out.used_live_gateway = false;

    // Fresh production pipeline each run — deterministic restart.
    MarketCorePipeline pipeline;
    pipeline.set_operating_mode(OperatingMode::Replay);
    if (paper_ != nullptr) {
        paper_->reset();
        pipeline.bind_order_gateway(*paper_);
    }
    if (equity_set_) pipeline.set_account_equity(equity_);
    if (weights_.has_value()) {
        apply_weight_bundle(pipeline, *weights_);
    }

    for (const auto& item : episode.market) {
        out.clock_order.push_back(item.domain);
        switch (item.domain) {
            case EpisodeClockDomain::RawQuote:
                ++out.raw_count;
                if (item.quote.has_value()) {
                    pipeline.process_event(*item.quote);
                }
                break;
            case EpisodeClockDomain::ClosedTenSecond:
                ++out.closed_10s_count;
                // Reinject recorded CLOSED 10s into the same production micro path.
                if (item.candle.has_value()) {
                    pipeline.process_closed_10s(*item.candle, item.ts);
                }
                break;
            case EpisodeClockDomain::AuthorityOhlc:
                ++out.authority_count;
                if (item.candle.has_value()) {
                    pipeline.process_authority_ohlc(*item.candle, item.timeframe);
                }
                break;
        }
    }

    out.final_brain = pipeline.brain_snapshot();
    out.final_micro = pipeline.micro().snapshot();
    out.open_positions = pipeline.open_positions();
    out.pending_intents = pipeline.pending_intents();
    return out;
}

}  // namespace mr
