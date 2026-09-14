#include "mr/execution_engine/execution_engine.hpp"
#include "mr/common/clock.hpp"
#include <algorithm>

namespace mr {

ExecutionEngine::ExecutionEngine(OrderGateway& gateway, ExecutionWeightConfig cfg)
    : gateway_(gateway), cfg_(std::move(cfg)) {}

void ExecutionEngine::set_weight_config(ExecutionWeightConfig cfg) { cfg_ = std::move(cfg); }

std::uint64_t ExecutionEngine::dedup_key(InstrumentId instrument, Direction direction) {
    return (static_cast<std::uint64_t>(instrument) << 8)
           | static_cast<std::uint64_t>(direction);
}

bool ExecutionEngine::was_recent_duplicate(InstrumentId instrument,
                                           Direction direction,
                                           Timestamp now) const {
    if (cfg_.dedup_window_ms == 0) return false;
    const auto it = recent_submits_.find(dedup_key(instrument, direction));
    if (it == recent_submits_.end()) return false;
    const double age_ms = static_cast<double>((now - it->second).count()) / 1'000'000.0;
    return age_ms >= 0.0 && age_ms < static_cast<double>(cfg_.dedup_window_ms);
}

ExecutionReport ExecutionEngine::submit(const TradeIntent& intent, double quantity) {
    ExecutionReport rep;
    rep.intent_id = intent.id;
    rep.instrument = intent.instrument;
    rep.direction = intent.direction;
    rep.requested_quantity = quantity;

    // Lifecycle guards only — never a trading thesis decision.
    if (intent.decision != EntryDecision::EntryReady) {
        rep.status = ExecutionStatus::Rejected;
        rep.reason_codes.push_back("INTENT_NOT_READY");
        rep.explanation = "Intent not entry-ready";
        return rep;
    }
    if (cfg_.require_positive_quantity && !(quantity > 0.0)) {
        rep.status = ExecutionStatus::Rejected;
        rep.reason_codes.push_back("INVALID_QUANTITY");
        rep.explanation = "Non-positive quantity";
        return rep;
    }
    if (!gateway_.healthy()) {
        rep.status = ExecutionStatus::Rejected;
        rep.reason_codes.push_back("BROKER_UNHEALTHY");
        rep.explanation = "Broker unhealthy";
        return rep;
    }
    if (was_recent_duplicate(intent.instrument, intent.direction, intent.created_at)) {
        rep.status = ExecutionStatus::Rejected;
        rep.reason_codes.push_back("DUPLICATE_ORDER");
        rep.explanation = "Duplicate submit suppressed";
        return rep;
    }

    auto req = builder_.from_intent(intent, quantity);
    CapitalOrderResponse resp;
    rep.status = ExecutionStatus::Submitted;

    const std::uint32_t attempts = std::max<std::uint32_t>(1, cfg_.max_attempts);
    for (std::uint32_t i = 0; i < attempts; ++i) {
        rep.attempts = i + 1;
        if (i > 0) rep.status = ExecutionStatus::Retrying;
        resp = gateway_.create_position(req);
        rep.last_response = resp;
        if (resp.success) {
            fills_.record({resp, now_utc_ns()});
            recent_submits_[dedup_key(intent.instrument, intent.direction)] = intent.created_at;
            rep.status = ExecutionStatus::Filled;
            rep.deal_id = resp.deal_id;
            rep.fill_price = resp.fill_price > 0.0 ? resp.fill_price : intent.reference_price;
            rep.filled_quantity =
                resp.filled_quantity > 0.0 ? resp.filled_quantity : quantity;
            rep.explanation = "filled";
            return rep;
        }
        (void)cfg_.backoff_ms;  // reserved for live runtime pacing
    }

    rep.status = ExecutionStatus::Rejected;
    rep.reason_codes.push_back("REJECTED");
    if (!resp.error_message.empty()) rep.reason_codes.push_back(resp.error_message);
    rep.explanation = resp.error_message.empty() ? "rejected" : resp.error_message;
    return rep;
}

ExecutionReport ExecutionEngine::close(const std::string& deal_id, TradeIntentId intent_id) {
    ExecutionReport rep;
    rep.intent_id = intent_id;
    rep.status = ExecutionStatus::Submitted;
    if (deal_id.empty()) {
        rep.status = ExecutionStatus::Rejected;
        rep.reason_codes.push_back("MISSING_DEAL_ID");
        return rep;
    }
    if (!gateway_.healthy()) {
        rep.status = ExecutionStatus::Rejected;
        rep.reason_codes.push_back("BROKER_UNHEALTHY");
        return rep;
    }
    auto resp = gateway_.close_position(deal_id);
    rep.last_response = resp;
    rep.attempts = 1;
    if (resp.success) {
        fills_.record({resp, now_utc_ns()});
        rep.status = ExecutionStatus::Filled;
        rep.deal_id = deal_id;
        rep.explanation = "closed";
    } else {
        rep.status = ExecutionStatus::Rejected;
        rep.reason_codes.push_back("CLOSE_REJECTED");
        rep.explanation = resp.error_message;
    }
    return rep;
}

}  // namespace mr
