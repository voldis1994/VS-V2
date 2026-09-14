#include "mr/market_core/pipeline.hpp"
#include <cmath>
#include <algorithm>

namespace mr {

MarketCorePipeline::MarketCorePipeline() : normalizer_(clock_), decision_(intent_ids_) {}

void MarketCorePipeline::configure(const ConfigRegistry& config) {
    if (!config.feeds().empty()) {
        stale_ms_ = config.feeds().front().stale_threshold_ms;
    }
    brain_runtime_.configure_from_env();
}

void MarketCorePipeline::bind_order_gateway(OrderGateway& gateway) {
    // Caller responsibility: Replay/Paper must use PaperOrderGateway — never Capital LIVE.
    execution_ = std::make_unique<ExecutionEngine>(gateway);
    broker_healthy_ = gateway.healthy();
}

void MarketCorePipeline::set_operating_mode(OperatingMode mode) {
    mode_ = mode;
    brain_runtime_.set_operating_mode(mode);
    if (mode_ == OperatingMode::Replay) {
        execution_.reset();
    }
}

void MarketCorePipeline::attach_episode_recorder(EpisodeRecorder* recorder) {
    recorder_ = recorder;
}

void MarketCorePipeline::set_account_equity(double equity) {
    if (equity > 0.0) account_equity_ = equity;
    else account_equity_.reset();
}

void MarketCorePipeline::clear_account_equity() { account_equity_.reset(); }

bool MarketCorePipeline::broker_healthy() const {
    if (!execution_) return false;
    return broker_healthy_;
}

void MarketCorePipeline::hydrate_open_positions(std::vector<PositionState> recovered) {
    open_positions_.clear();
    for (auto& pos : recovered) {
        if (!(pos.quantity > 0.0) || pos.deal_id.empty()) continue;
        open_positions_.push_back(std::move(pos));
    }
}

namespace {
const char* health_status_name(HealthStatus s) {
    switch (s) {
        case HealthStatus::Healthy: return "HEALTHY";
        case HealthStatus::Degraded: return "DEGRADED";
        case HealthStatus::Unhealthy: return "UNHEALTHY";
        case HealthStatus::Disconnected: return "DISCONNECTED";
    }
    return "UNKNOWN";
}
}  // namespace

void MarketCorePipeline::publish_brain_feed() {
    BrainFeedRuntime runtime;

    for (const auto& c : health_.snapshot()) {
        if (c.name == "market_core") runtime.market_core_health = health_status_name(c.status);
        else if (c.name == "feeds") runtime.feeds_health = health_status_name(c.status);
        else if (c.name == "execution") runtime.execution_health = health_status_name(c.status);
        else if (c.name == "data") runtime.data_health = health_status_name(c.status);
    }

    // Measured open-book exposure (0 when flat is authentic, not a hardcoded placeholder).
    ExposureState exp{};
    for (const auto& pos : open_positions_) {
        if (!(pos.quantity > 0.0)) continue;
        const double px = pos.current_price > 0.0 ? pos.current_price : pos.entry_price;
        const double notional = std::abs(pos.quantity * px);
        exp.gross += notional;
        exp.net += (pos.direction == Direction::Short ? -notional : notional);
        ++exp.open_positions;
    }
    runtime.exposure = exp.gross;
    risk_.set_exposure(exp);

    if (auto v = risk_.daily_pnl()) runtime.daily_pnl = *v;
    if (auto v = risk_.max_drawdown()) runtime.max_drawdown = *v;
    if (auto v = risk_.realized_pnl()) runtime.realized_pnl = *v;

    brain_runtime_.observe(brain_.snapshot(), runtime);
}

void MarketCorePipeline::process_event(const MarketEvent& event) {
    telemetry_.record_event();

    if (recorder_ != nullptr && recorder_->active()) {
        recorder_->record_raw_quote(event);
    }

    // RAW QUOTE domain
    auto norm = normalizer_.normalize(event);
    quality_.process(norm, stale_ms_);
    auto health = quality_.health(norm.source);
    health_.heartbeat("market_core", HealthStatus::Healthy);
    health_.heartbeat("feeds", health.status);
    health_.heartbeat("data", health.status);
    fusion_.ingest(norm, health);
    auto consensus = fusion_.consensus(norm.instrument);

    // Candle engine may emit FORMING / CLOSED_10s / derived CLOSED_1m (not authority).
    auto clock_events = brain_.on_normalized(norm, consensus);

    perception_.on_event(norm, consensus.mid);
    auto pd = perception_.dynamics();
    micro_.update(norm);

    handle_clock_events(clock_events, pd, consensus, norm.instrument);

    // Open positions keep receiving market updates — PositionBrain management only.
    if (!open_positions_.empty()) {
        DualPrediction dual{};
        const auto it = last_dual_.find(norm.instrument);
        if (it != last_dual_.end()) {
            dual = it->second;
        } else {
            const auto snap = brain_.snapshot();
            const auto bit = snap.instruments.find(norm.instrument);
            if (bit != snap.instruments.end() && bit->second.has_prediction) {
                dual = bit->second.prediction;
            }
        }
        manage_open_positions(norm.instrument, dual, pd, consensus.mid, norm.normalized_timestamp);
    }
}

void MarketCorePipeline::process_closed_10s(const Candle& closed, Timestamp ts) {
    const Timestamp at = ts.count() > 0 ? ts : closed.open_time;
    if (recorder_ != nullptr && recorder_->active()) {
        recorder_->record_closed_10s(closed, at, /*one_shot=*/true);
    }
    // CLOSED 10s one-shot — microstructure authority only (never structure).
    micro_.on_closed_10s(closed);
    const InstrumentId instrument =
        closed.instrument != kInvalidInstrument ? closed.instrument : 1;
    brain_.apply_micro_evidence(instrument, micro_.snapshot(), at);

    publish_brain_feed();
}

void MarketCorePipeline::process_authority_ohlc(const Candle& closed, Timeframe tf) {
    if (recorder_ != nullptr && recorder_->active()) {
        recorder_->record_authority_ohlc(closed, tf, closed.open_time);
    }
    // Capital closed 1m+ — the ONLY structure/context authority.
    auto& ce = brain_.candles(closed.instrument != kInvalidInstrument ? closed.instrument : 1);
    auto events = ce.ingest_authority_ohlc(closed, tf);
    auto pd = perception_.dynamics();

    for (const auto& ev : events) {
        if (!is_structure_authority(ev) || !ev.candle.has_value()) continue;
        structure_.on_authority_close(*ev.candle, ev.timeframe, pd);
        auto st = structure_.snapshot();
        auto rg = concepts_.evaluate(pd, st);
        brain_.apply_authority_structure(ev.instrument, st, rg, ev.ts);
        ConsensusQuote consensus;
        consensus.mid = ev.candle->close;
        consensus.spread = 0;
        consensus.sources = 1;
        consensus.confidence = 1.0;
        // Decision + risk (+ execution when gateway bound) after structure update.
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
                break;
            case MarketClockKind::FormingCandle:
                break;
            case MarketClockKind::ClosedTenSecond:
                if (!ev.one_shot || !ev.candle.has_value()) break;
                // Same production path as process_closed_10s() (replay reinject).
                if (recorder_ != nullptr && recorder_->active()) {
                    recorder_->record_closed_10s(*ev.candle, ev.ts, /*one_shot=*/true);
                }
                micro_.on_closed_10s(*ev.candle);
                brain_.apply_micro_evidence(instrument, micro_.snapshot(), ev.ts);
                break;
            case MarketClockKind::ClosedOneMinute:
            case MarketClockKind::ClosedHigherTimeframe:
                if (ev.structure_authority && ev.candle.has_value()) {
                    structure_.on_authority_close(*ev.candle, ev.timeframe, pd);
                    auto st = structure_.snapshot();
                    auto rg = concepts_.evaluate(pd, st);
                    brain_.apply_authority_structure(instrument, st, rg, ev.ts);
                    run_decision_and_risk(st, pd, consensus, instrument);
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

    // Stage 5: prediction from structure + CLOSED 10s micro (concepts are context, not triggers).
    const auto micro_snap = micro_.snapshot();
    auto dual = prediction_.evaluate(st, structure_.has_authority(), micro_snap, pd);
    last_dual_[instrument] = dual;
    brain_.apply_prediction(instrument, dual, /*ts*/ Timestamp{});

    // Position management on every authority cycle — not a second entry DecisionEngine.
    manage_open_positions(instrument, dual, pd, consensus.mid, Timestamp{});

    // DecisionEngine is the sole BUY/SELL/WAIT source — relative LONG vs SHORT EV.
    auto sides = decision_.evaluate(dual, consensus.spread, instrument);
    brain_.apply_decision(instrument, sides.chosen, sides.final_action, Timestamp{});

    Quote q;
    q.instrument = instrument;
    q.spread.bid = consensus.mid - consensus.spread / 2.0;
    q.spread.ask = consensus.mid + consensus.spread / 2.0;
    q.valid = consensus.valid() || consensus.mid > 0;

    // RAW quote = execution/safety only inside decide().
    auto intent = decision_.decide(sides.chosen, q);
    if (intent.decision != EntryDecision::EntryReady) {
        telemetry_.record_decision();
        return;
    }

    // One live position per instrument — no pyramiding via a second brain.
    for (const auto& p : open_positions_) {
        if (p.instrument == instrument && p.quantity > 0.0) {
            telemetry_.record_decision();
            return;
        }
    }

    RiskRequest req;
    req.intent = intent;
    req.mid_price = consensus.mid > 0 ? consensus.mid : intent.reference_price;
    // Real spread cost — RiskEngine may veto on cost / width; never invents a default.
    req.spread = consensus.spread;
    req.spread_cost = consensus.spread;
    req.data_fresh = consensus.valid() || consensus.mid > 0;
    // Pending (no gateway) is allowed; LIVE execution path stays fail-closed on broker health.
    req.broker_healthy = has_execution() ? broker_healthy() : true;
    if (account_equity_.has_value()) {
        req.account_equity = *account_equity_;
    } else {
        req.account_equity = 0;  // risk fail-closed
    }

    auto risk = risk_.evaluate(req);
    brain_.apply_risk(instrument, risk, Timestamp{});
    if (!risk.approved) {
        telemetry_.record_decision();
        return;
    }

    risk_.remember_order(intent.instrument, intent.direction, intent.created_at);

    if (execution_) {
        execute_entry(intent, risk, dual, Timestamp{});
    } else {
        pending_.push_back(intent);
    }
    telemetry_.record_decision();

    publish_brain_feed();
}

void MarketCorePipeline::execute_entry(const TradeIntent& intent,
                                       const RiskDecision& risk,
                                       const DualPrediction& dual,
                                       Timestamp ts) {
    if (!execution_) return;
    const double qty = risk.approved_quantity > 0.0 ? risk.approved_quantity : 0.0;
    auto exec = execution_->submit(intent, qty);
    brain_.apply_execution(intent.instrument, exec, ts);
    if (exec.status != ExecutionStatus::Filled) {
        return;
    }

    const auto thesis = PositionBrain::side_for(dual, intent.direction);
    auto pos = position_.open(intent, exec.fill_price, exec.filled_quantity, thesis);
    pos.deal_id = exec.deal_id;
    PositionDecision hold;
    hold.action = PositionAction::Hold;
    hold.reason_codes.push_back("OPEN");
    brain_.apply_position(intent.instrument, pos, hold, ts);
    open_positions_.push_back(std::move(pos));
    if (recorder_ != nullptr && recorder_->active()) {
        recorder_->on_entry(open_positions_.back(), ts);
        const auto snap = brain_.snapshot();
        const auto it = snap.instruments.find(intent.instrument);
        if (it != snap.instruments.end()) {
            recorder_->record_brain_frame(it->second, EpisodeClockDomain::AuthorityOhlc, ts);
        }
    }
}

void MarketCorePipeline::manage_open_positions(InstrumentId instrument,
                                               const DualPrediction& dual,
                                               const PriceDynamics& pd,
                                               double mid,
                                               Timestamp ts) {
    if (!(mid > 0.0) || open_positions_.empty()) return;

    for (auto& pos : open_positions_) {
        if (pos.instrument != instrument || !(pos.quantity > 0.0)) continue;
        pos.current_price = mid;
        const auto side = PositionBrain::side_for(dual, pos.direction);
        auto decision = position_.evaluate(pos, side, pd);
        if (decision.action == PositionAction::Protect && decision.suggested_stop > 0.0) {
            pos.stop_loss = decision.suggested_stop;
        }
        apply_position_action(pos, decision, mid, ts);
        brain_.apply_position(instrument, pos, decision, ts);
    }

    open_positions_.erase(
        std::remove_if(open_positions_.begin(), open_positions_.end(),
                       [](const PositionState& p) { return !(p.quantity > 0.0); }),
        open_positions_.end());
}

void MarketCorePipeline::apply_position_action(PositionState& pos,
                                               const PositionDecision& decision,
                                               double mid,
                                               Timestamp ts) {
    if (!execution_) return;

    if (decision.action == PositionAction::Exit) {
        auto exec = execution_->close(pos.deal_id, pos.intent_id);
        brain_.apply_execution(pos.instrument, exec, ts);
        if (exec.status == ExecutionStatus::Filled) {
            if (recorder_ != nullptr && recorder_->active()) {
                recorder_->on_exit(pos, decision, mid, ts);
            }
            pos.quantity = 0.0;
        }
        return;
    }

    if (decision.action == PositionAction::Reduce) {
        const double frac = std::clamp(decision.reduce_fraction, 0.0, 1.0);
        const double qty = pos.quantity * frac;
        if (!(qty > 0.0)) return;
        auto exec = execution_->reduce(pos, qty, mid);
        brain_.apply_execution(pos.instrument, exec, ts);
        if (exec.status == ExecutionStatus::Filled) {
            pos.quantity = std::max(0.0, pos.quantity - exec.filled_quantity);
        }
    }
}

std::vector<TradeIntent> MarketCorePipeline::drain_pending_intents() {
    auto out = pending_;
    pending_.clear();
    return out;
}

bool MarketCorePipeline::enter_from_decision(const TradeIntent& intent,
                                             const DualPrediction& dual,
                                             double mid,
                                             double spread) {
    if (intent.decision != EntryDecision::EntryReady) return false;

    for (const auto& p : open_positions_) {
        if (p.instrument == intent.instrument && p.quantity > 0.0) return false;
    }

    last_dual_[intent.instrument] = dual;

    RiskRequest req;
    req.intent = intent;
    req.mid_price = mid > 0.0 ? mid : intent.reference_price;
    req.spread = spread;
    req.spread_cost = spread;
    req.data_fresh = mid > 0.0;
    // Pending (no gateway) is allowed; LIVE execution path stays fail-closed on broker health.
    req.broker_healthy = has_execution() ? broker_healthy() : true;
    if (account_equity_.has_value()) {
        req.account_equity = *account_equity_;
    } else {
        req.account_equity = 0;
    }

    auto risk = risk_.evaluate(req);
    brain_.apply_risk(intent.instrument, risk, intent.created_at);
    if (!risk.approved) return false;

    risk_.remember_order(intent.instrument, intent.direction, intent.created_at);

    if (!execution_) {
        pending_.push_back(intent);
        return true;
    }

    execute_entry(intent, risk, dual, intent.created_at);
    return !open_positions_.empty()
           && open_positions_.back().instrument == intent.instrument
           && open_positions_.back().quantity > 0.0;
}

void MarketCorePipeline::update_open_positions(InstrumentId instrument,
                                               const DualPrediction& dual,
                                               const PriceDynamics& pd,
                                               double mid) {
    last_dual_[instrument] = dual;
    manage_open_positions(instrument, dual, pd, mid, Timestamp{});
}

}  // namespace mr
