#include "mr/decision/decision_engine.hpp"
#include "mr/common/clock.hpp"
#include <algorithm>
#include <cmath>

namespace mr {
namespace {

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

double soft01(double raw, double scale) {
    const double x = std::max(0.0, raw);
    if (scale <= 1e-15) return clamp01(x);
    return clamp01(1.0 - std::exp(-x / scale));
}

double wavg2(double a, double b, double wa, double wb) {
    const double den = std::max(1e-15, wa + wb);
    return (wa * a + wb * b) / den;
}

}  // namespace

DecisionEngine::DecisionEngine(IdGenerator& ids, DecisionWeightConfig cfg)
    : ids_(ids), cfg_(std::move(cfg)) {}

void DecisionEngine::set_weight_config(DecisionWeightConfig cfg) { cfg_ = std::move(cfg); }

Opportunity DecisionEngine::from_side(const SidePrediction& side,
                                      double spread_cost,
                                      InstrumentId instrument) const {
    Opportunity o;
    o.instrument = instrument;
    o.direction = side.direction;
    o.probability = side.probability;
    o.spread_cost = spread_cost;
    const double cost = std::max(0.0, spread_cost) * cfg_.cost_scale;
    o.expected_value = side.expected_value - cost;
    o.action = TradeAction::Wait;
    return o;
}

void DecisionEngine::apply_geometry(Opportunity& opp,
                                    const SidePrediction& side,
                                    const DualPrediction& dual) const {
    const double stop_raw =
        cfg_.stop_adverse_weight * side.adverse_move
        + cfg_.stop_vol_weight * dual.structure_volatility
        + cfg_.stop_invalidation_weight
              * clamp01(side.invalidation + dual.structure_invalidation);
    const double target_raw =
        cfg_.target_expected_weight * side.expected_move
        + cfg_.target_vol_weight * dual.structure_volatility;

    opp.stop_distance_frac =
        soft01(stop_raw, cfg_.stop_distance_scale) * cfg_.stop_move_frac;
    opp.target_distance_frac =
        soft01(target_raw, cfg_.target_distance_scale) * cfg_.target_move_frac;
}

SideEvaluation DecisionEngine::evaluate(const DualPrediction& dual,
                                        double spread_cost,
                                        InstrumentId instrument) const {
    SideEvaluation eval;
    eval.long_opp = from_side(dual.long_side, spread_cost, instrument);
    eval.short_opp = from_side(dual.short_side, spread_cost, instrument);

    const double net_l = eval.long_opp.expected_value;
    const double net_s = eval.short_opp.expected_value;
    const double edge_l = net_l - net_s;  // >0 favors LONG
    const double edge_s = -edge_l;

    const double ql = dual.long_side.thesis_quality;
    const double qs = dual.short_side.thesis_quality;
    const double cont_l = dual.long_side.continuation;
    const double cont_s = dual.short_side.continuation;

    // Authority / evidence isolation → WAIT (not a confidence threshold).
    if (!dual.evidence_sufficient || !dual.has_structure_authority || !dual.has_micro_authority) {
        eval.wait_score = 1.0;
        eval.buy_score = 0.0;
        eval.sell_score = 0.0;
        eval.chosen = net_l >= net_s ? eval.long_opp : eval.short_opp;
        eval.chosen.action = TradeAction::Wait;
        eval.chosen.direction = Direction::Flat;
        eval.chosen.reason_codes.push_back("INSUFFICIENT_EVIDENCE_WAIT");
        eval.final_action = TradeAction::Wait;
        return eval;
    }

    // Relative EV clarity — Stage-8 calibrates edge_scale; no absolute BUY cutoffs.
    const double edge_clarity = soft01(std::abs(edge_l), cfg_.edge_scale);

    // Continuous relative side strength: EV edge × quality/continuation evidence.
    // No hardcoded 0.35/0.65 floors — mix weights are Stage-8 calibratable.
    const double long_thesis = wavg2(ql, cont_l, cfg_.w_quality, cfg_.w_continuation);
    const double short_thesis = wavg2(qs, cont_s, cfg_.w_quality, cfg_.w_continuation);
    const double long_mass = std::max(0.0, edge_l) * long_thesis;
    const double short_mass = std::max(0.0, edge_s) * short_thesis;
    eval.buy_score = soft01(long_mass, cfg_.edge_scale);
    eval.sell_score = soft01(short_mass, cfg_.edge_scale);

    // Conflict: both theses meaningful while relative edge is unclear.
    const double conflict_raw = std::min(ql, qs) * (1.0 - edge_clarity);
    // Weakness: low best thesis and/or both net EVs non-positive.
    const double both_weak_ev = (net_l <= 0.0 && net_s <= 0.0) ? 1.0 : 0.0;
    const double weak_raw =
        cfg_.weak_quality_weight * (1.0 - std::max(ql, qs)) + cfg_.weak_ev_weight * both_weak_ev;

    // Wait pressure shrinks as one side's relative EV becomes clear.
    eval.wait_score = (soft01(conflict_raw, cfg_.conflict_scale)
                       + soft01(weak_raw, cfg_.weakness_scale))
                      * (1.0 - edge_clarity);

    if (eval.wait_score >= eval.buy_score && eval.wait_score >= eval.sell_score) {
        eval.chosen = net_l >= net_s ? eval.long_opp : eval.short_opp;
        eval.chosen.action = TradeAction::Wait;
        eval.chosen.direction = Direction::Flat;
        if (conflict_raw >= weak_raw) {
            eval.chosen.reason_codes.push_back("CONFLICTING_EVIDENCE_WAIT");
        } else {
            eval.chosen.reason_codes.push_back("WEAK_EVIDENCE_WAIT");
        }
        eval.final_action = TradeAction::Wait;
        return eval;
    }

    if (eval.buy_score > eval.sell_score) {
        eval.chosen = eval.long_opp;
        eval.chosen.action = TradeAction::Buy;
        eval.chosen.direction = Direction::Long;
        apply_geometry(eval.chosen, dual.long_side, dual);
        eval.final_action = TradeAction::Buy;
    } else if (eval.sell_score > eval.buy_score) {
        eval.chosen = eval.short_opp;
        eval.chosen.action = TradeAction::Sell;
        eval.chosen.direction = Direction::Short;
        apply_geometry(eval.chosen, dual.short_side, dual);
        eval.final_action = TradeAction::Sell;
    } else {
        eval.chosen = eval.long_opp;
        eval.chosen.action = TradeAction::Wait;
        eval.chosen.direction = Direction::Flat;
        eval.chosen.reason_codes.push_back("LONG_SHORT_TIE_WAIT");
        eval.final_action = TradeAction::Wait;
    }
    return eval;
}

SideEvaluation DecisionEngine::evaluate_long_short_wait(const Scenario& sc,
                                                        PredictionEngine& prediction,
                                                        const PriceDynamics& pd,
                                                        double spread_cost) {
    DualPrediction dual;
    dual.has_structure_authority = true;
    dual.has_micro_authority = true;
    dual.evidence_sufficient = sc.confidence > 0.0;  // soft presence, not a BUY gate
    dual.structure_volatility = 0.0;
    dual.structure_invalidation = clamp01(sc.invalidation);

    auto pl = prediction.predict(sc, pd, Direction::Long);
    auto ps = prediction.predict(sc, pd, Direction::Short);

    dual.long_side.direction = Direction::Long;
    dual.long_side.probability = pl.probability;
    dual.long_side.uncertainty = pl.uncertainty;
    dual.long_side.expected_move = pl.expected_mfe;
    dual.long_side.adverse_move = pl.expected_mae;
    dual.long_side.expected_value =
        pl.probability * pl.expected_mfe - (1.0 - pl.probability) * pl.expected_mae;
    dual.long_side.continuation = clamp01(pl.probability);
    dual.long_side.reversal_failure = clamp01(1.0 - pl.probability);
    dual.long_side.confidence = clamp01(1.0 - pl.uncertainty);
    dual.long_side.invalidation = clamp01(sc.invalidation);
    dual.long_side.thesis_quality =
        clamp01(dual.long_side.confidence * dual.long_side.continuation * sc.confidence);

    dual.short_side.direction = Direction::Short;
    dual.short_side.probability = ps.probability;
    dual.short_side.uncertainty = ps.uncertainty;
    dual.short_side.expected_move = ps.expected_mfe;
    dual.short_side.adverse_move = ps.expected_mae;
    dual.short_side.expected_value =
        ps.probability * ps.expected_mfe - (1.0 - ps.probability) * ps.expected_mae;
    dual.short_side.continuation = clamp01(ps.probability);
    dual.short_side.reversal_failure = clamp01(1.0 - ps.probability);
    dual.short_side.confidence = clamp01(1.0 - ps.uncertainty);
    dual.short_side.invalidation = clamp01(sc.invalidation);
    dual.short_side.thesis_quality =
        clamp01(dual.short_side.confidence * dual.short_side.continuation * sc.confidence);

    if (sc.confidence <= 0.0) {
        dual.evidence_sufficient = false;
    }

    return evaluate(dual, spread_cost);
}

TradeIntent DecisionEngine::decide(const Opportunity& opp, const Quote& quote, std::uint64_t ttl_ms) {
    TradeIntent t;
    t.id = ids_.generate();
    t.instrument = opp.instrument;
    t.direction = opp.direction;
    t.created_at = now_utc_ns();
    t.expires_at = Timestamp(t.created_at.count() + static_cast<long long>(ttl_ms) * 1'000'000LL);
    t.probability = opp.probability;
    t.expected_value = opp.expected_value;

    // RAW quote = execution / safety only.
    if (!quote.valid) {
        t.decision = EntryDecision::Reject;
        t.reason_codes.push_back("NO_QUOTE");
        t.explanation = "Missing quote — execution safety reject";
        return t;
    }

    t.reference_price = quote.spread.mid_price();
    const double stop_frac = std::max(0.0, opp.stop_distance_frac);
    const double target_frac = std::max(0.0, opp.target_distance_frac);

    if (opp.action == TradeAction::Buy) {
        t.decision = EntryDecision::EntryReady;
        t.direction = Direction::Long;
        t.stop_loss = t.reference_price * (1.0 - stop_frac);
        t.take_profit = t.reference_price * (1.0 + target_frac);
        t.explanation = "BUY from relative LONG EV dominance";
    } else if (opp.action == TradeAction::Sell) {
        t.decision = EntryDecision::EntryReady;
        t.direction = Direction::Short;
        t.stop_loss = t.reference_price * (1.0 + stop_frac);
        t.take_profit = t.reference_price * (1.0 - target_frac);
        t.explanation = "SELL from relative SHORT EV dominance";
    } else {
        t.decision = EntryDecision::NoTrade;
        t.direction = Direction::Flat;
        t.explanation = "WAIT";
        t.reason_codes = opp.reason_codes;
    }
    return t;
}

}  // namespace mr
