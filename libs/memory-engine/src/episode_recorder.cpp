#include "mr/memory_engine/episode_recorder.hpp"

#include <algorithm>
#include <cstdint>

namespace mr {
namespace {

std::uint64_t fnv1a64(const void* data, std::size_t n) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    std::uint64_t h = 14695981039346656037ull;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= bytes[i];
        h *= 1099511628211ull;
    }
    return h;
}

std::string to_hex64(std::uint64_t v) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[static_cast<std::size_t>(i)] = kHex[v & 0xf];
        v >>= 4;
    }
    return out;
}

double favorable(Direction dir, double anchor, double mid) {
    if (dir == Direction::Long) return mid - anchor;
    if (dir == Direction::Short) return anchor - mid;
    return 0.0;
}

double adverse(Direction dir, double anchor, double mid) {
    return -favorable(dir, anchor, mid);
}

Timestamp event_ts(const MarketEvent& event) {
    if (event.receive_timestamp.count() > 0) return event.receive_timestamp;
    if (event.exchange_timestamp.count() > 0) return event.exchange_timestamp;
    return event.provider_timestamp;
}

}  // namespace

void EpisodeRecorder::set_provenance(EpisodeProvenance provenance) {
    episode_.provenance = std::move(provenance);
}

void EpisodeRecorder::set_model_id(std::string model_id) {
    episode_.provenance.model_id = std::move(model_id);
}

void EpisodeRecorder::set_config_hash(std::string config_hash) {
    episode_.provenance.config_hash = std::move(config_hash);
}

void EpisodeRecorder::set_weight_hashes(const PredictionWeightConfig& prediction,
                                        const DecisionWeightConfig& decision,
                                        const RiskWeightConfig& risk,
                                        const ExecutionWeightConfig& execution,
                                        const PositionWeightConfig& position) {
    episode_.provenance.prediction_weights_hash = hash_pod(prediction);
    episode_.provenance.decision_weights_hash = hash_pod(decision);
    episode_.provenance.risk_weights_hash = hash_pod(risk);
    episode_.provenance.execution_weights_hash = hash_pod(execution);
    episode_.provenance.position_weights_hash = hash_pod(position);
}

std::string EpisodeRecorder::hash_bytes(const void* data, std::size_t n) {
    return to_hex64(fnv1a64(data, n));
}

void EpisodeRecorder::begin_episode(std::string episode_id, InstrumentId instrument) {
    EpisodeProvenance keep = std::move(episode_.provenance);
    episode_ = TradeEpisode{};
    episode_.provenance = std::move(keep);
    episode_.provenance.brain_version = kBrainVersion;
    episode_.episode_id = std::move(episode_id);
    episode_.instrument = instrument;
    active_ = true;
    in_post_exit_ = false;
    entry_price_ = 0;
    entry_direction_ = Direction::Flat;
    seq_ = 0;
}

void EpisodeRecorder::record_raw_quote(const MarketEvent& event) {
    if (!active_ || episode_.sealed) return;
    EpisodeMarketItem item;
    item.domain = EpisodeClockDomain::RawQuote;
    item.ts = event_ts(event);
    item.sequence = ++seq_;
    item.instrument = event.instrument;
    item.quote = event;
    item.timeframe = Timeframe::Second1;
    episode_.market.push_back(item);

    double mid = 0.0;
    if (event.last.has_value()) mid = *event.last;
    else if (event.bid.has_value() && event.ask.has_value())
        mid = (*event.bid + *event.ask) * 0.5;
    if (!(mid > 0.0)) return;
    if (in_post_exit_) on_post_exit_mark(mid, item.ts);
    else if (episode_.outcome.entry_ts.count() > 0 && !episode_.outcome.sealed)
        on_mark(mid, item.ts);
}

void EpisodeRecorder::record_closed_10s(const Candle& candle, Timestamp ts, bool one_shot) {
    if (!active_ || episode_.sealed) return;
    EpisodeMarketItem item;
    item.domain = EpisodeClockDomain::ClosedTenSecond;
    item.ts = ts.count() > 0 ? ts : candle.open_time;
    item.sequence = ++seq_;
    item.instrument = candle.instrument;
    item.candle = candle;
    item.timeframe = Timeframe::Second10;
    item.one_shot = one_shot;
    episode_.market.push_back(std::move(item));
}

void EpisodeRecorder::record_authority_ohlc(const Candle& candle, Timeframe tf, Timestamp ts) {
    if (!active_ || episode_.sealed) return;
    EpisodeMarketItem item;
    item.domain = EpisodeClockDomain::AuthorityOhlc;
    item.ts = ts.count() > 0 ? ts : candle.open_time;
    item.sequence = ++seq_;
    item.instrument = candle.instrument;
    item.candle = candle;
    item.timeframe = tf;
    item.structure_authority = true;
    item.one_shot = true;
    episode_.market.push_back(std::move(item));
}

void EpisodeRecorder::record_brain_frame(const BrainContext& ctx,
                                         EpisodeClockDomain trigger,
                                         Timestamp ts) {
    if (!active_ || episode_.sealed) return;
    EpisodeBrainFrame frame;
    frame.ts = ts.count() > 0 ? ts : ctx.ts;
    frame.trigger = trigger;
    frame.instrument = ctx.instrument;
    frame.structure = ctx.structure;
    frame.regime = ctx.regime;
    frame.micro = ctx.micro;
    frame.prediction = ctx.prediction;
    frame.decision = ctx.decision;
    frame.decision_action = ctx.decision_action;
    frame.risk = ctx.risk;
    frame.execution = ctx.execution;
    frame.position = ctx.position;
    frame.position_decision = ctx.position_decision;
    frame.has_structure_authority = ctx.has_structure_authority;
    frame.has_micro_authority = ctx.has_micro_authority;
    frame.has_prediction = ctx.has_prediction;
    frame.has_decision = ctx.has_decision;
    frame.has_risk = ctx.has_risk;
    frame.has_execution = ctx.has_execution;
    frame.has_position = ctx.has_position;
    episode_.frames.push_back(std::move(frame));
}

void EpisodeRecorder::record_position_action(const PositionDecision& decision) {
    if (!active_ || episode_.sealed) return;
    episode_.position_actions.push_back(decision);
}

void EpisodeRecorder::on_entry(const PositionState& pos, Timestamp ts) {
    if (!active_ || episode_.sealed) return;
    entry_price_ = pos.entry_price;
    entry_direction_ = pos.direction;
    episode_.outcome.direction = pos.direction;
    episode_.outcome.entry_price = pos.entry_price;
    episode_.outcome.quantity = pos.quantity;
    episode_.outcome.entry_ts = ts.count() > 0 ? ts : pos.opened_at;
    episode_.outcome.mfe = pos.mfe;
    episode_.outcome.mae = pos.mae;
    episode_.outcome.peak_retention = pos.peak_retention;
    episode_.outcome.sealed = false;
    in_post_exit_ = false;
}

void EpisodeRecorder::on_mark(double mid, Timestamp /*ts*/) {
    if (!active_ || episode_.sealed || !(entry_price_ > 0.0)) return;
    const double fav = favorable(entry_direction_, entry_price_, mid);
    const double adv = adverse(entry_direction_, entry_price_, mid);
    if (fav > episode_.outcome.mfe) episode_.outcome.mfe = fav;
    if (adv > episode_.outcome.mae) episode_.outcome.mae = adv;
    if (episode_.outcome.mfe > 0.0) {
        episode_.outcome.peak_retention = std::max(0.0, fav) / episode_.outcome.mfe;
    }
}

void EpisodeRecorder::on_exit(const PositionState& pos_at_exit,
                              const PositionDecision& exit_decision,
                              double exit_price,
                              Timestamp ts) {
    if (!active_ || episode_.sealed) return;
    episode_.outcome.exit_price = exit_price;
    episode_.outcome.exit_ts = ts;
    episode_.outcome.exit_action = exit_decision.action;
    episode_.outcome.exit_reason = exit_decision.reason;
    episode_.outcome.mfe = std::max(episode_.outcome.mfe, pos_at_exit.mfe);
    episode_.outcome.mae = std::max(episode_.outcome.mae, pos_at_exit.mae);
    episode_.outcome.peak_retention = pos_at_exit.peak_retention;
    if (entry_direction_ == Direction::Long) {
        episode_.outcome.realized_pnl =
            (exit_price - entry_price_) * episode_.outcome.quantity;
    } else if (entry_direction_ == Direction::Short) {
        episode_.outcome.realized_pnl =
            (entry_price_ - exit_price) * episode_.outcome.quantity;
    }
    record_position_action(exit_decision);
    in_post_exit_ = true;
}

void EpisodeRecorder::on_post_exit_mark(double mid, Timestamp ts) {
    if (!active_ || !in_post_exit_ || episode_.sealed) return;
    const double exit_px = episode_.outcome.exit_price;
    if (!(exit_px > 0.0)) return;
    const double fav = favorable(entry_direction_, exit_px, mid);
    const double adv = adverse(entry_direction_, exit_px, mid);
    if (fav > episode_.outcome.post_exit_favorable)
        episode_.outcome.post_exit_favorable = fav;
    if (adv > episode_.outcome.post_exit_adverse)
        episode_.outcome.post_exit_adverse = adv;
    episode_.outcome.post_exit_end_ts = ts;
    ++episode_.outcome.post_exit_samples;
}

void EpisodeRecorder::seal_episode() {
    if (!active_) return;
    episode_.outcome.sealed = true;
    episode_.sealed = true;
    in_post_exit_ = false;
    active_ = false;
}

TradeEpisode EpisodeRecorder::take_episode() {
    TradeEpisode out = std::move(episode_);
    episode_ = TradeEpisode{};
    active_ = false;
    in_post_exit_ = false;
    return out;
}

}  // namespace mr
