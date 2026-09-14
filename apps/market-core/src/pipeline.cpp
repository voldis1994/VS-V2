#include "mr/market_core/pipeline.hpp"
namespace mr {
MarketCorePipeline::MarketCorePipeline() : normalizer_(clock_), decision_(intent_ids_) {}
void MarketCorePipeline::configure(const ConfigRegistry& config) {
    stale_ms_ = config.get_double("stale_threshold_ms", 500.0);
}
void MarketCorePipeline::process_event(const MarketEvent& event) {
    telemetry_.record_event();
    auto norm = normalizer_.normalize(event);
    quality_.process(norm, stale_ms_);
    auto health = quality_.health(norm.source);
    fusion_.ingest(norm, health);
    auto consensus = fusion_.consensus(norm.instrument);
    brain_.on_normalized(norm, consensus);
    perception_.on_event(norm, consensus.mid);
    auto pd = perception_.dynamics();
    structure_.update(consensus.mid, pd, brain_.candles(norm.instrument).state());
    auto st = structure_.snapshot();
    auto rg = concepts_.evaluate(pd, st);
    micro_.update(norm);
    auto scens = scenarios_.evaluate(st, rg, micro_.snapshot());
    if (scens.empty()) return;
    auto pred = prediction_.predict(scens[0], pd, Direction::Long);
    auto opp = decision_.evaluate_opportunity(scens[0], pred, consensus.spread, Direction::Long);
    Quote q; q.instrument = norm.instrument; q.spread.bid = consensus.mid - consensus.spread/2;
    q.spread.ask = consensus.mid + consensus.spread/2; q.valid = consensus.valid();
    auto intent = decision_.decide(opp, q);
    if (intent.decision == EntryDecision::EntryReady) pending_.push_back(intent);
    telemetry_.record_decision();
}
std::vector<TradeIntent> MarketCorePipeline::drain_pending_intents() {
    auto out = pending_; pending_.clear(); return out;
}
}