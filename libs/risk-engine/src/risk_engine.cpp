#include "mr/risk/risk_engine.hpp"
#include <algorithm>
#include <cmath>

namespace mr {

RiskEngine::RiskEngine(RiskWeightConfig cfg) : cfg_(std::move(cfg)) {}

RiskEngine::RiskEngine(RiskLimits limits)
    : cfg_(limits.to_weight_config()), emergency_stop_(limits.emergency_stop) {}

void RiskEngine::set_weight_config(RiskWeightConfig cfg) { cfg_ = std::move(cfg); }

std::uint64_t RiskEngine::order_key(InstrumentId instrument, Direction direction) {
    return (static_cast<std::uint64_t>(instrument) << 8)
           | static_cast<std::uint64_t>(direction);
}

void RiskEngine::remember_order(InstrumentId instrument, Direction direction, Timestamp ts) {
    recent_orders_[order_key(instrument, direction)] = ts;
}

SizingResult RiskEngine::size_position(const TradeIntent& intent,
                                       double equity,
                                       double price) const {
    SizingResult r;
    if (emergency_stop_) { r.reason = "EMERGENCY_STOP"; return r; }
    if (!(equity > 0.0) || !(price > 0.0)) { r.reason = "MISSING_ACCOUNT_EQUITY"; return r; }
    if (intent.decision != EntryDecision::EntryReady) { r.reason = "NOT_READY"; return r; }

    const double stop_dist = std::abs(price - intent.stop_loss);
    const double risk_budget = equity * std::max(0.0, cfg_.risk_budget_frac);
    double qty = (stop_dist > 0.0 && risk_budget > 0.0) ? (risk_budget / stop_dist) : 0.0;

    const double max_notional = equity * std::max(0.0, cfg_.max_position_notional_frac);
    if (max_notional > 0.0) {
        const double cap = max_notional / price;
        qty = qty > 0.0 ? std::min(qty, cap) : cap;
    }
    if (cfg_.max_quantity > 0.0) qty = std::min(qty, cfg_.max_quantity);

    r.quantity = std::max(0.0, qty);
    r.approved = r.quantity > 0.0;
    if (!r.approved) r.reason = "ZERO_SIZE";
    return r;
}

GuardResult RiskEngine::pre_trade_check(const RiskRequest& request) const {
    GuardResult g;
    if (emergency_stop_) { g.pass = false; g.reason = "EMERGENCY_STOP"; return g; }
    if (!(request.account_equity > 0.0)) { g.pass = false; g.reason = "MISSING_ACCOUNT_EQUITY"; return g; }
    if (!request.broker_healthy) { g.pass = false; g.reason = "BROKER_FAILURE"; return g; }
    if (!request.data_fresh) { g.pass = false; g.reason = "STALE_DATA"; return g; }
    if (cfg_.max_quote_age_ms > 0.0 && request.quote_age_ms > cfg_.max_quote_age_ms) {
        g.pass = false; g.reason = "STALE_QUOTE"; return g;
    }
    if (request.duplicate_order) { g.pass = false; g.reason = "DUPLICATE_ORDER"; return g; }

    const double mid = request.mid_price > 0.0 ? request.mid_price : request.intent.reference_price;
    if (mid > 0.0 && request.spread > 0.0 && cfg_.max_spread_frac > 0.0) {
        if ((request.spread / mid) > cfg_.max_spread_frac) {
            g.pass = false; g.reason = "SPREAD_TOO_WIDE"; return g;
        }
    }

    const double equity = request.account_equity;
    const double daily_pnl = request.daily_pnl != 0.0 ? request.daily_pnl : daily_pnl_;
    if (cfg_.max_daily_loss_frac > 0.0 && daily_pnl <= -cfg_.max_daily_loss_frac * equity) {
        g.pass = false; g.reason = "DAILY_LOSS"; return g;
    }

    const double gross = std::max(request.open_gross_notional, exposure_.gross);
    const double net = request.open_net_notional != 0.0 ? request.open_net_notional : exposure_.net;
    const double opens = std::max(request.open_position_count,
                                  static_cast<double>(exposure_.open_positions));
    if (cfg_.max_gross_exposure_frac > 0.0 && gross > cfg_.max_gross_exposure_frac * equity) {
        g.pass = false; g.reason = "GROSS_EXPOSURE"; return g;
    }
    if (cfg_.max_net_exposure_frac > 0.0 && std::abs(net) > cfg_.max_net_exposure_frac * equity) {
        g.pass = false; g.reason = "NET_EXPOSURE"; return g;
    }
    if (cfg_.max_open_positions > 0.0 && opens >= cfg_.max_open_positions) {
        g.pass = false; g.reason = "MAX_OPEN_POSITIONS"; return g;
    }

    const double spread_cost = std::max(0.0, request.spread_cost) * cfg_.spread_cost_scale;
    const double net_ev = request.intent.expected_value - spread_cost;
    if (cfg_.min_net_ev_scale > 0.0 && net_ev * cfg_.min_net_ev_scale <= 0.0) {
        g.pass = false; g.reason = "NEGATIVE_NET_EV"; return g;
    }

    if (cfg_.duplicate_window_ms > 0.0) {
        const auto key = order_key(request.intent.instrument, request.intent.direction);
        const auto it = recent_orders_.find(key);
        if (it != recent_orders_.end()) {
            const double age_ms =
                static_cast<double>((request.intent.created_at - it->second).count()) / 1'000'000.0;
            if (age_ms >= 0.0 && age_ms < cfg_.duplicate_window_ms) {
                g.pass = false; g.reason = "DUPLICATE_ORDER"; return g;
            }
        }
    }
    return g;
}

GuardResult RiskEngine::monitor_position(const PositionState& pos, double daily_pnl) {
    (void)pos;
    daily_pnl_ = daily_pnl;
    has_daily_pnl_ = true;
    GuardResult g;
    if (emergency_stop_) { g.pass = false; g.reason = "EMERGENCY_STOP"; }
    return g;
}

RiskDecision RiskEngine::evaluate(const RiskRequest& request) {
    RiskDecision out;
    out.id = request.intent.id;
    out.instrument = request.intent.instrument;
    out.direction = request.intent.direction;
    out.reference_price =
        request.mid_price > 0.0 ? request.mid_price : request.intent.reference_price;
    out.confidence = request.intent.probability;
    out.type = RiskIntentType::Entry;
    out.max_risk_fraction = request.account_risk_budget > 0.0 ? request.account_risk_budget
                                                              : cfg_.risk_budget_frac;

    auto guard = pre_trade_check(request);
    if (!guard.pass) {
        out.approved = false;
        out.reason_codes.push_back(guard.reason);
        out.human_explanation = guard.reason;
        return out;
    }

    auto size = size_position(request.intent, request.account_equity, out.reference_price);
    out.approved_quantity = size.quantity;
    if (request.account_equity > 0.0 && out.reference_price > 0.0) {
        out.size_fraction = (size.quantity * out.reference_price) / request.account_equity;
    }
    out.approved = size.approved;
    if (!out.approved) {
        out.reason_codes.push_back(size.reason.empty() ? "ZERO_SIZE" : size.reason);
        out.human_explanation = out.reason_codes.front();
        return out;
    }
    out.human_explanation = "approved";
    return out;
}

}  // namespace mr
