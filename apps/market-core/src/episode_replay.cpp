#include "mr/market_core/episode_replay.hpp"

namespace mr {

void EpisodeReplay::bind_paper_gateway(PaperOrderGateway& gateway) { paper_ = &gateway; }

void EpisodeReplay::set_account_equity(double equity) {
    equity_ = equity;
    equity_set_ = equity > 0.0;
}

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
                // CLOSED 10s remains a distinct clock domain in the episode record.
                // Production pipeline derives CLOSED 10s from RAW quotes via candle engine;
                // we preserve domain ordering here without collapsing clocks.
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
    out.open_positions = pipeline.open_positions();
    out.pending_intents = pipeline.pending_intents();
    return out;
}

}  // namespace mr
