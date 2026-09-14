#include "mr/position_brain/position_brain.hpp"
namespace mr {
double PositionBrain::pnl(const PositionState& p, double price) const {
    return p.direction == Direction::Long ? (price - p.entry_price) * p.quantity : (p.entry_price - price) * p.quantity;
}
PositionState PositionBrain::open(const TradeIntent& intent, double fill, double qty) {
    PositionState p; p.id = ids_.generate(); p.intent_id = intent.id; p.instrument = intent.instrument;
    p.direction = intent.direction; p.entry_price = fill; p.quantity = qty; p.current_price = fill;
    p.opened_at = intent.created_at; p.stop_loss = intent.stop_loss; p.take_profit = intent.take_profit;
    p.peak_favorable_price = fill; return p;
}
void PositionBrain::update_excursions(PositionState& pos, double price) {
    pos.current_price = price; pos.current_pnl = pnl(pos, price);
    double fav = pos.direction == Direction::Long ? price - pos.entry_price : pos.entry_price - price;
    double adv = pos.direction == Direction::Long ? pos.entry_price - price : price - pos.entry_price;
    if (fav > pos.mfe) { pos.mfe = fav; pos.peak_favorable_price = price; }
    if (adv > pos.mae) pos.mae = adv;
    if (pos.mfe > 0) {
        double cur = pos.direction == Direction::Long ? price - pos.entry_price : pos.entry_price - price;
        pos.peak_retention = cur / pos.mfe;
    }
}
PositionDecision PositionBrain::evaluate(PositionState& pos, const PriceDynamics& pd, const RegimeFeatures& rg) {
    update_excursions(pos, pos.current_price);
    PositionDecision d; d.continuation_probability = rg.confidence; d.reversal_probability = 1.0 - rg.confidence;
    d.ev_hold = pos.current_pnl; d.ev_exit = pos.current_pnl;
    if (pos.direction == Direction::Long && pos.current_price <= pos.stop_loss) {
        d.action = PositionAction::ExitNow; d.reason = ExitReason::HardInvalidation; d.reason_codes.push_back("STOP");
        return d;
    }
    if (pos.direction == Direction::Short && pos.current_price >= pos.stop_loss) {
        d.action = PositionAction::ExitNow; d.reason = ExitReason::HardInvalidation; d.reason_codes.push_back("STOP");
        return d;
    }
    if (pos.peak_retention < 0.5 && pos.mfe > 0) {
        d.action = PositionAction::Protect; d.reason = ExitReason::PeakProtection; d.reason_codes.push_back("PEAK_PROTECT");
        return d;
    }
    // Multi-concept context (descriptive scores — not entry triggers).
    if (pos.direction == Direction::Long && pd.velocity < 0
        && rg.score(Regime::TrendDown) >= rg.score(Regime::TrendUp)) {
        d.action = PositionAction::ExitNow; d.reason = ExitReason::ThesisFailure; return d;
    }
    if (pos.direction == Direction::Short && pd.velocity > 0
        && rg.score(Regime::TrendUp) >= rg.score(Regime::TrendDown)) {
        d.action = PositionAction::ExitNow; d.reason = ExitReason::ThesisFailure; return d;
    }
    if (pos.mfe > 0 && pos.current_pnl >= pos.mfe * 0.9) {
        d.action = PositionAction::TakeProfit; d.reason = ExitReason::Target; return d;
    }
    d.action = PositionAction::Hold; return d;
}
}