#include "mr/market_core/pipeline.hpp"

namespace mr {

MarketCorePipeline::MarketCorePipeline() : normalizer_(clock_), decision_(intent_ids_) {}

void MarketCorePipeline::configure(const ConfigRegistry& config) {
    if (!config.feeds().empty()) {
        stale_ms_ = config.feeds().front().stale_threshold_ms;
    }
}

void MarketCorePipeline::set_account_equity(double equity) {
    if (equity > 0.0) account_equity_ = equity;
    else account_equity_.reset();
}

void MarketCorePipeline::clear_account_equity() { account_equity_.reset(); }

void MarketCorePipeline::process_event(const MarketEvent& event) {
    telemetry_.record_event();

    // RAW QUOTE domain
    auto norm = normalizer_.normalize(event);
    quality_.process(norm, stale_ms_);
    auto health = quality_.health(norm.source);
    fusion_.ingest(norm, health);
    auto consensus = fusion_.consensus(norm.instrument);

    // Candle engine may emit FORMING / CLOSED_10s / derived CLOSED_1m (not authority).
    auto clock_events = brain_.on_normalized(norm, consensus);

    perception_.on_event(norm, consensus.mid);
    auto pd = perception_.dynamics();
    micro_.update(norm);

    handle_clock_events(clock_events, pd, consensus, norm.instrument);
}

void MarketCorePipeline::process_authority_ohlc(const Candle& closed, Timeframe tf) {
    // Capital closed 1m+ — the ONLY structure/context authority.
    auto& ce = brain_.candles(closed.instrument != kInvalidInstrument ? closed.instrument : 1);
    auto events = ce.ingest_authority_ohlc(closed, tf);
    auto pd = perception_.dynamics();

    for (const auto& ev : events) {
        if (!is_structure_authority(ev) || !ev.candle.has_value()) continue;
        structure_.on_authority_close(*ev.candle, ev.timeframe, pd);
        auto st = structure_.snapshot();
        ConsensusQuote consensus;
        consensus.mid = ev.candle->close;
        consensus.spread = 0;
        consensus.sources = 1;
        consensus.confidence = 1.0;
        // Decision runs on authority close after structure update.
        run_decision_and_risk(st, pd, consensus, ev.instrument);
    }
}

void MarketCorePipeline::handle_clock_events(const std::vector<MarketClockEvent>& events,
                                             const PriceDynamics& pd,
                                             const ConsensusQuote& consensus,
                                             InstrumentId instrument) {
    for (const auto& ev : events) {
        switch (ev.kind) {
            case MarketClockKind::RawQuote:
                // Already handled in process_event.
                break;
            case MarketClockKind::FormingCandle:
                // Forming is observable state only — no structure, no decision.
                break;
            case MarketClockKind::ClosedTenSecond:
                // One-shot 10s close: micro/timing domain only — NOT structure authority.
                if (!ev.one_shot) break;
                break;
            case MarketClockKind::ClosedOneMinute:
            case MarketClockKind::ClosedHigherTimeframe:
                // Quote-derived closes are not Capital authority; ignore for structure.
                if (ev.structure_authority && ev.candle.has_value()) {
                    structure_.on_authority_close(*ev.candle, ev.timeframe, pd);
                    run_decision_and_risk(structure_.snapshot(), pd, consensus, instrument);
                }
                break;
        }
    }
}

void MarketCorePipeline::run_decision_and_risk(const StructureFeatures& st,
                                               const PriceDynamics& pd,
                                               const ConsensusQuote& consensus,
                                               InstrumentId instrument) {
    if (!structure_.has_authority()) {
        // Fail closed: no structure authority => no trade decision promotion.
        return;
    }

    auto rg = concepts_.evaluate(pd, st);
    auto scens = scenarios_.evaluate(st, rg, micro_.snapshot());
    if (scens.empty()) return;

    // LONG + SHORT + WAIT — never hardcode Long.
    auto sides = decision_.evaluate_long_short_wait(scens[0], prediction_, pd, consensus.spread);
    Quote q;
    q.instrument = instrument;
    q.spread.bid = consensus.mid - consensus.spread / 2.0;
    q.spread.ask = consensus.mid + consensus.spread / 2.0;
    q.valid = consensus.valid() || consensus.mid > 0;

    auto intent = decision_.decide(sides.chosen, q);
    if (intent.decision != EntryDecision::EntryReady) {
        telemetry_.record_decision();
        return;
    }

    RiskRequest req;
    req.intent = intent;
    req.mid_price = consensus.mid > 0 ? consensus.mid : intent.reference_price;
    if (account_equity_.has_value()) {
        req.account_equity = *account_equity_;
    } else {
        req.account_equity = 0;  // risk fail-closed
    }

    auto risk = risk_.evaluate(req);
    if (!risk.approved) {
        telemetry_.record_decision();
        return;
    }

    pending_.push_back(intent);
    telemetry_.record_decision();
}

std::vector<TradeIntent> MarketCorePipeline::drain_pending_intents() {
    auto out = pending_;
    pending_.clear();
    return out;
}

}  // namespace mr
