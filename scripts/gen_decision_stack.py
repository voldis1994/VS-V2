#!/usr/bin/env python3
from pathlib import Path
import textwrap
ROOT = Path("/workspace")
def w(p, c):
    path = ROOT / p
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(textwrap.dedent(c).lstrip("\n"))

# memory-engine
w("libs/memory-engine/include/mr/memory_engine/memory_types.hpp", """
#pragma once
#include "mr/common/id.hpp"
#include <deque>
#include <string>
#include <vector>
namespace mr {
struct WorkingMemory { std::deque<double> recent_returns; std::size_t max_size{64}; };
struct SessionMemory { double session_high{0}, session_low{0}, std::uint64_t event_count{0}; };
struct StructuralMemory { double last_swing_high{0}, last_swing_low{0}; };
struct HistoricalMemory { std::deque<double> closes; };
struct PatternMemory { std::vector<std::vector<double>> clusters; };
struct PredictionMemory { double last_prob{0.5}, last_ev{0}; };
struct TradeMemory { std::uint64_t wins{0}, losses{0}; double avg_win{0}, avg_loss{0}; };
struct MemoryBank {
    WorkingMemory working; SessionMemory session; StructuralMemory structural;
    HistoricalMemory historical; PatternMemory patterns; PredictionMemory prediction; TradeMemory trades;
};
}""")

w("libs/memory-engine/include/mr/memory_engine/memory_engine.hpp", """
#pragma once
#include "mr/memory_engine/memory_types.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
namespace mr {
class MemoryEngine {
public:
    void update(double price, const PriceDynamics& pd, bool trade_closed, double pnl);
    [[nodiscard]] const MemoryBank& bank() const { return bank_; }
private:
    MemoryBank bank_;
};
}""")

w("libs/memory-engine/src/memory_engine.cpp", """
#include "mr/memory_engine/memory_engine.hpp"
namespace mr {
void MemoryEngine::update(double price, const PriceDynamics& pd, bool trade_closed, double pnl) {
    bank_.working.recent_returns.push_back(pd.normalized_return);
    if (bank_.working.recent_returns.size() > bank_.working.max_size) bank_.working.recent_returns.pop_front();
    bank_.session.event_count++;
    if (bank_.session.session_high == 0 || price > bank_.session.session_high) bank_.session.session_high = price;
    if (bank_.session.session_low == 0 || price < bank_.session.session_low) bank_.session.session_low = price;
    bank_.historical.closes.push_back(price);
    if (bank_.historical.closes.size() > 500) bank_.historical.closes.pop_front();
    if (pd.velocity > 0 && price > bank_.structural.last_swing_high) bank_.structural.last_swing_high = price;
    if (pd.velocity < 0 && (bank_.structural.last_swing_low == 0 || price < bank_.structural.last_swing_low))
        bank_.structural.last_swing_low = price;
    if (trade_closed) {
        if (pnl >= 0) { bank_.trades.wins++; bank_.trades.avg_win = (bank_.trades.avg_win * (bank_.trades.wins-1) + pnl) / bank_.trades.wins; }
        else { bank_.trades.losses++; bank_.trades.avg_loss = (bank_.trades.avg_loss * (bank_.trades.losses-1) + std::abs(pnl)) / bank_.trades.losses; }
    }
}
}""")

w("libs/memory-engine/CMakeLists.txt", """
add_library(mr_memory_engine STATIC src/memory_engine.cpp)
add_library(mr::memory-engine ALIAS mr_memory_engine)
target_include_directories(mr_memory_engine PUBLIC include)
target_link_libraries(mr_memory_engine PUBLIC mr::perception-engine)
target_compile_features(mr_memory_engine PUBLIC cxx_std_20)
""")

# pattern-engine
w("libs/pattern-engine/include/mr/pattern_engine/pattern_vector.hpp", """
#pragma once
#include <vector>
namespace mr { using PatternVector = std::vector<double>; }""")

w("libs/pattern-engine/include/mr/pattern_engine/pattern_encoder.hpp", """
#pragma once
#include "mr/pattern_engine/pattern_vector.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/structure_engine/structure_features.hpp"
namespace mr {
class PatternEncoder {
public:
    PatternVector encode(const PriceDynamics& pd, const StructureFeatures& st) const;
};
}""")

w("libs/pattern-engine/include/mr/pattern_engine/pattern_similarity.hpp", """
#pragma once
#include "mr/pattern_engine/pattern_vector.hpp"
namespace mr {
class PatternSimilarity {
public:
    [[nodiscard]] double cosine(const PatternVector& a, const PatternVector& b) const;
};
}""")

w("libs/pattern-engine/include/mr/pattern_engine/pattern_cluster.hpp", """
#pragma once
#include "mr/pattern_engine/pattern_vector.hpp"
#include <vector>
namespace mr {
class PatternCluster {
public:
    std::size_t assign(const PatternVector& v, double threshold = 0.85);
    [[nodiscard]] const std::vector<PatternVector>& centroids() const { return centroids_; }
private:
    std::vector<PatternVector> centroids_;
};
}""")

w("libs/pattern-engine/include/mr/pattern_engine/pattern_statistics.hpp", """
#pragma once
#include "mr/pattern_engine/pattern_vector.hpp"
namespace mr {
struct PatternStats { double mean_similarity{0}; std::size_t cluster_count{0}; };
class PatternStatistics {
public:
    PatternStats compute(const std::vector<PatternVector>& patterns) const;
};
}""")

w("libs/pattern-engine/include/mr/pattern_engine/pattern_engine.hpp", """
#pragma once
#include "mr/pattern_engine/pattern_encoder.hpp"
#include "mr/pattern_engine/pattern_similarity.hpp"
#include "mr/pattern_engine/pattern_cluster.hpp"
#include "mr/pattern_engine/pattern_statistics.hpp"
namespace mr {
class PatternEngine {
public:
    std::size_t observe(const PriceDynamics& pd, const StructureFeatures& st);
    [[nodiscard]] PatternStats stats() const;
private:
    PatternEncoder encoder_; PatternSimilarity sim_; PatternCluster cluster_;
    std::vector<PatternVector> history_;
};
}""")

w("libs/pattern-engine/src/pattern_encoder.cpp", """
#include "mr/pattern_engine/pattern_encoder.hpp"
namespace mr {
PatternVector PatternEncoder::encode(const PriceDynamics& pd, const StructureFeatures& st) const {
    return {pd.normalized_return, pd.velocity, pd.acceleration, pd.directional_persistence,
            st.range_position, st.continuation_pressure, st.pullback_depth, st.breakout_strength};
}
}""")

w("libs/pattern-engine/src/pattern_similarity.cpp", """
#include "mr/pattern_engine/pattern_similarity.hpp"
#include <cmath>
#include <numeric>
namespace mr {
double PatternSimilarity::cosine(const PatternVector& a, const PatternVector& b) const {
    if (a.size() != b.size() || a.empty()) return 0;
    double dot=0, na=0, nb=0;
    for (std::size_t i=0;i<a.size();++i) { dot += a[i]*b[i]; na += a[i]*a[i]; nb += b[i]*b[i]; }
    if (na <= 0 || nb <= 0) return 0;
    return dot / (std::sqrt(na)*std::sqrt(nb));
}
}""")

w("libs/pattern-engine/src/pattern_cluster.cpp", """
#include "mr/pattern_engine/pattern_cluster.hpp"
#include "mr/pattern_engine/pattern_similarity.hpp"
namespace mr {
std::size_t PatternCluster::assign(const PatternVector& v, double threshold) {
    PatternSimilarity sim;
    for (std::size_t i=0;i<centroids_.size();++i)
        if (sim.cosine(v, centroids_[i]) >= threshold) return i;
    centroids_.push_back(v);
    return centroids_.size()-1;
}
}""")

w("libs/pattern-engine/src/pattern_statistics.cpp", """
#include "mr/pattern_engine/pattern_statistics.hpp"
#include "mr/pattern_engine/pattern_similarity.hpp"
namespace mr {
PatternStats PatternStatistics::compute(const std::vector<PatternVector>& patterns) const {
    PatternStats s; s.cluster_count = patterns.size();
    if (patterns.size() < 2) return s;
    PatternSimilarity sim; double sum=0; std::size_t n=0;
    for (std::size_t i=1;i<patterns.size();++i) { sum += sim.cosine(patterns[i-1], patterns[i]); n++; }
    s.mean_similarity = n ? sum/n : 0; return s;
}
}""")

w("libs/pattern-engine/src/pattern_engine.cpp", """
#include "mr/pattern_engine/pattern_engine.hpp"
namespace mr {
std::size_t PatternEngine::observe(const PriceDynamics& pd, const StructureFeatures& st) {
    auto v = encoder_.encode(pd, st); history_.push_back(v);
    if (history_.size() > 1000) history_.erase(history_.begin());
    return cluster_.assign(v);
}
PatternStats PatternEngine::stats() const {
    PatternStatistics ps; return ps.compute(history_);
}
}""")

w("libs/pattern-engine/CMakeLists.txt", """
add_library(mr_pattern_engine STATIC
    src/pattern_encoder.cpp src/pattern_similarity.cpp src/pattern_cluster.cpp
    src/pattern_statistics.cpp src/pattern_engine.cpp)
add_library(mr::pattern-engine ALIAS mr_pattern_engine)
target_include_directories(mr_pattern_engine PUBLIC include)
target_link_libraries(mr_pattern_engine PUBLIC mr::perception-engine mr::structure-engine mr::memory-engine)
target_compile_features(mr_pattern_engine PUBLIC cxx_std_20)
""")

# scenario-engine
w("libs/scenario-engine/include/mr/scenario_engine/scenario_type.hpp", """
#pragma once
namespace mr {
enum class ScenarioType : std::uint8_t {
    Continuation=0, Pullback=1, Range=2, Breakout=3, Failure=4, Reversal=5
};
}""")

w("libs/scenario-engine/include/mr/scenario_engine/scenario.hpp", """
#pragma once
#include "mr/scenario_engine/scenario_type.hpp"
namespace mr {
struct Scenario { ScenarioType type{ScenarioType::Range}; double confidence{0}; double target_move{0}; double invalidation{0}; };
}""")

w("libs/scenario-engine/include/mr/scenario_engine/scenario_engine.hpp", """
#pragma once
#include "mr/scenario_engine/scenario.hpp"
#include "mr/structure_engine/structure_features.hpp"
#include "mr/market_concepts/regime_features.hpp"
#include "mr/microstructure_engine/microstructure_features.hpp"
namespace mr {
class ScenarioEngine {
public:
    std::vector<Scenario> evaluate(const StructureFeatures& st, const RegimeFeatures& rg, const MicrostructureFeatures& ms);
};
}""")

w("libs/scenario-engine/src/scenario_engine.cpp", """
#include "mr/scenario_engine/scenario_engine.hpp"
namespace mr {
std::vector<Scenario> ScenarioEngine::evaluate(const StructureFeatures& st, const RegimeFeatures& rg, const MicrostructureFeatures& ms) {
    std::vector<Scenario> out;
    if (st.continuation_pressure > 0.5 && rg.current == Regime::TrendUp)
        out.push_back({ScenarioType::Continuation, st.continuation_pressure, st.range_width * 0.5, st.range_width * 0.2});
    if (st.pullback_depth > 0.4)
        out.push_back({ScenarioType::Pullback, st.pullback_depth, st.range_width * 0.3, st.pullback_depth * st.range_width});
    if (rg.current == Regime::Range)
        out.push_back({ScenarioType::Range, rg.confidence, st.range_width * 0.25, st.range_boundary_distance});
    if (st.breakout_strength > 0.5)
        out.push_back({ScenarioType::Breakout, st.breakout_strength, st.range_width, st.range_width * 0.15});
    if (st.failed_breakout > 0.3 || ms.exhaustion_proxy > 0.5)
        out.push_back({ScenarioType::Failure, std::max(st.failed_breakout, ms.exhaustion_proxy), st.range_width * 0.2, 0});
    if (st.reversal_candidate > 0.3 || ms.rejection_proxy > 0.5)
        out.push_back({ScenarioType::Reversal, std::max(st.reversal_candidate, ms.rejection_proxy), st.range_width * 0.4, st.range_width * 0.2});
    return out;
}
}""")

w("libs/scenario-engine/CMakeLists.txt", """
add_library(mr_scenario_engine STATIC src/scenario_engine.cpp)
add_library(mr::scenario-engine ALIAS mr_scenario_engine)
target_include_directories(mr_scenario_engine PUBLIC include)
target_link_libraries(mr_scenario_engine PUBLIC mr::structure-engine mr::market-concepts mr::microstructure-engine)
target_compile_features(mr_scenario_engine PUBLIC cxx_std_20)
""")

# prediction-engine
w("libs/prediction-engine/include/mr/prediction_engine/prediction.hpp", """
#pragma once
namespace mr {
struct Prediction {
    double probability{0.5}, uncertainty{0.5};
    double expected_mfe{0}, expected_mae{0}, expected_duration_s{0};
};
}""")

w("libs/prediction-engine/include/mr/prediction_engine/prediction_engine.hpp", """
#pragma once
#include "mr/prediction_engine/prediction.hpp"
#include "mr/scenario_engine/scenario.hpp"
#include "mr/pattern_engine/pattern_engine.hpp"
#include "mr/common/id.hpp"
namespace mr {
class PredictionEngine {
public:
    Prediction predict(const Scenario& scenario, const PriceDynamics& pd, Direction dir);
};
}""")

w("libs/prediction-engine/src/prediction_engine.cpp", """
#include "mr/prediction_engine/prediction_engine.hpp"
#include <cmath>
namespace mr {
Prediction PredictionEngine::predict(const Scenario& scenario, const PriceDynamics& pd, Direction dir) {
    Prediction p;
    p.probability = std::clamp(0.5 + scenario.confidence * 0.3, 0.05, 0.95);
    if (dir == Direction::Long && pd.directional_persistence < 0) p.probability -= 0.15;
    if (dir == Direction::Short && pd.directional_persistence > 0) p.probability -= 0.15;
    p.uncertainty = 1.0 - std::abs(pd.directional_persistence);
    p.expected_mfe = scenario.target_move;
    p.expected_mae = scenario.invalidation;
    p.expected_duration_s = 30.0 + scenario.confidence * 60.0;
    return p;
}
}""")

w("libs/prediction-engine/CMakeLists.txt", """
add_library(mr_prediction_engine STATIC src/prediction_engine.cpp)
add_library(mr::prediction-engine ALIAS mr_prediction_engine)
target_include_directories(mr_prediction_engine PUBLIC include)
target_link_libraries(mr_prediction_engine PUBLIC mr::scenario-engine mr::pattern-engine mr::perception-engine)
target_compile_features(mr_prediction_engine PUBLIC cxx_std_20)
""")

# decision-engine
w("libs/decision-engine/include/mr/decision_engine/decision_types.hpp", """
#pragma once
#include "mr/common/id.hpp"
#include <string>
#include <vector>
namespace mr {
enum class TradeAction : std::uint8_t { Wait=0, Buy=1, Sell=2 };
enum class EntryDecision : std::uint8_t { NoTrade=0, EntryReady=1, Reject=2 };
struct Opportunity {
    InstrumentId instrument{kInvalidInstrument};
    Direction direction{Direction::Flat};
    double probability{0}, expected_value{0}, spread_cost{0};
    TradeAction action{TradeAction::Wait};
    std::vector<std::string> reason_codes;
};
struct TradeIntent {
    TradeIntentId id{0}; InstrumentId instrument{kInvalidInstrument};
    Direction direction{Direction::Flat}; Timestamp created_at{}, expires_at{};
    double reference_price{0}, probability{0}, expected_value{0}, stop_loss{0}, take_profit{0};
    EntryDecision decision{EntryDecision::NoTrade};
    std::vector<std::string> reason_codes;
    std::string explanation;
};
}""")

w("libs/decision-engine/include/mr/decision_engine/decision_engine.hpp", """
#pragma once
#include "mr/decision_engine/decision_types.hpp"
#include "mr/prediction_engine/prediction.hpp"
#include "mr/scenario_engine/scenario.hpp"
#include "mr/market_types/quote.hpp"
namespace mr {
class DecisionEngine {
public:
    explicit DecisionEngine(IdGenerator& ids) : ids_(ids) {}
    Opportunity evaluate_opportunity(const Scenario& sc, const Prediction& pred, double spread_cost, Direction dir);
    TradeIntent decide(const Opportunity& opp, const Quote& quote, std::uint64_t ttl_ms = 2000);
private:
    IdGenerator& ids_;
    double compute_ev(double prob, double win, double loss, double cost) const;
};
}""")

w("libs/decision-engine/src/decision_engine.cpp", """
#include "mr/decision_engine/decision_engine.hpp"
#include "mr/common/clock.hpp"
namespace mr {
double DecisionEngine::compute_ev(double prob, double win, double loss, double cost) const {
    return prob * win - (1.0 - prob) * loss - cost;
}
Opportunity DecisionEngine::evaluate_opportunity(const Scenario& sc, const Prediction& pred, double spread_cost, Direction dir) {
    Opportunity o; o.direction = dir;
    o.probability = pred.probability;
    o.spread_cost = spread_cost;
    o.expected_value = compute_ev(pred.probability, pred.expected_mfe, pred.expected_mae, spread_cost);
    if (o.expected_value > spread_cost && pred.probability > 0.55) {
        o.action = dir == Direction::Long ? TradeAction::Buy : TradeAction::Sell;
    } else {
        o.action = TradeAction::Wait;
        o.reason_codes.push_back("LOW_EV");
    }
    if (sc.confidence < 0.3) { o.action = TradeAction::Wait; o.reason_codes.push_back("LOW_SCENARIO_CONF"); }
    return o;
}
TradeIntent DecisionEngine::decide(const Opportunity& opp, const Quote& quote, std::uint64_t ttl_ms) {
    TradeIntent t; t.id = ids_.generate(); t.instrument = opp.instrument; t.direction = opp.direction;
    t.created_at = now_utc_ns(); t.expires_at = Timestamp(t.created_at.count() + static_cast<long long>(ttl_ms) * 1'000'000LL);
    t.probability = opp.probability; t.expected_value = opp.expected_value;
    if (!quote.valid) { t.decision = EntryDecision::Reject; t.reason_codes.push_back("NO_QUOTE"); return t; }
    t.reference_price = quote.spread.mid_price();
    if (opp.action == TradeAction::Buy) {
        t.decision = EntryDecision::EntryReady;
        t.stop_loss = t.reference_price * 0.998; t.take_profit = t.reference_price * 1.004;
        t.explanation = "BUY opportunity";
    } else if (opp.action == TradeAction::Sell) {
        t.decision = EntryDecision::EntryReady;
        t.stop_loss = t.reference_price * 1.002; t.take_profit = t.reference_price * 0.996;
        t.explanation = "SELL opportunity";
    } else {
        t.decision = EntryDecision::NoTrade;
        t.explanation = "WAIT";
        t.reason_codes = opp.reason_codes;
    }
    return t;
}
}""")

w("libs/decision-engine/CMakeLists.txt", """
add_library(mr_decision_engine STATIC src/decision_engine.cpp)
add_library(mr::decision-engine ALIAS mr_decision_engine)
target_include_directories(mr_decision_engine PUBLIC include)
target_link_libraries(mr_decision_engine PUBLIC mr::prediction-engine mr::market-types)
target_compile_features(mr_decision_engine PUBLIC cxx_std_20)
""")

# position-brain
w("libs/position-brain/include/mr/position_brain/position_types.hpp", """
#pragma once
#include "mr/decision_engine/decision_types.hpp"
namespace mr {
enum class ExitReason : std::uint8_t { None=0, ThesisFailure=1, HardInvalidation=2, PeakProtection=3, Target=4, TimeDecay=5, ReversalEvidence=6, EmergencyStop=7 };
enum class PositionAction : std::uint8_t { Hold=0, ExitNow=1, Trail=2, TakeProfit=3, Reduce=4, Protect=5 };
struct PositionState {
    PositionId id{0}; TradeIntentId intent_id{0}; InstrumentId instrument{kInvalidInstrument};
    Direction direction{Direction::Flat}; double entry_price{0}, quantity{0}, current_price{0};
    Timestamp opened_at{}; double mfe{0}, mae{0}, peak_favorable_price{0}, peak_retention{0};
    double current_pnl{0}, stop_loss{0}, take_profit{0}; std::uint64_t horizon_ns{10'000'000'000};
};
struct PositionDecision {
    PositionAction action{PositionAction::Hold}; ExitReason reason{ExitReason::None};
    double ev_exit{0}, ev_hold{0}, continuation_probability{0}, reversal_probability{0};
    std::vector<std::string> reason_codes;
};
}""")

w("libs/position-brain/include/mr/position_brain/position_brain.hpp", """
#pragma once
#include "mr/position_brain/position_types.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/market_concepts/regime_features.hpp"
namespace mr {
class PositionBrain {
public:
    PositionState open(const TradeIntent& intent, double fill, double qty);
    PositionDecision evaluate(PositionState& pos, const PriceDynamics& pd, const RegimeFeatures& rg);
    void update_excursions(PositionState& pos, double price);
private:
    IdGenerator ids_;
    double pnl(const PositionState& p, double price) const;
};
}""")

w("libs/position-brain/src/position_brain.cpp", """
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
    if (pos.direction == Direction::Long && pd.velocity < 0 && rg.current == Regime::TrendDown) {
        d.action = PositionAction::ExitNow; d.reason = ExitReason::ThesisFailure; return d;
    }
    if (pos.direction == Direction::Short && pd.velocity > 0 && rg.current == Regime::TrendUp) {
        d.action = PositionAction::ExitNow; d.reason = ExitReason::ThesisFailure; return d;
    }
    if (pos.mfe > 0 && pos.current_pnl >= pos.mfe * 0.9) {
        d.action = PositionAction::TakeProfit; d.reason = ExitReason::Target; return d;
    }
    d.action = PositionAction::Hold; return d;
}
}""")

w("libs/position-brain/CMakeLists.txt", """
add_library(mr_position_brain STATIC src/position_brain.cpp)
add_library(mr::position-brain ALIAS mr_position_brain)
target_include_directories(mr_position_brain PUBLIC include)
target_link_libraries(mr_position_brain PUBLIC mr::decision-engine mr::market-concepts mr::perception-engine)
target_compile_features(mr_position_brain PUBLIC cxx_std_20)
""")

# risk-engine
w("libs/risk-engine/include/mr/risk_engine/risk_types.hpp", """
#pragma once
namespace mr {
struct RiskLimits { double max_position_size{1.0}; double max_daily_loss{1000}; double max_drawdown_pct{5}; };
struct SizingResult { double quantity{0}; bool approved{false}; std::string reason; };
struct GuardResult { bool pass{true}; std::string reason; };
}""")

w("libs/risk-engine/include/mr/risk_engine/risk_engine.hpp", """
#pragma once
#include "mr/risk_engine/risk_types.hpp"
#include "mr/decision_engine/decision_types.hpp"
#include "mr/position_brain/position_types.hpp"
namespace mr {
class RiskEngine {
public:
    explicit RiskEngine(RiskLimits limits) : limits_(limits) {}
    SizingResult size_position(const TradeIntent& intent, double balance, double price);
    GuardResult pre_trade_check(const TradeIntent& intent, double spread_cost);
    GuardResult monitor_position(const PositionState& pos, double daily_pnl);
    bool emergency_stop{false};
private:
    RiskLimits limits_;
    double daily_pnl_{0};
};
}""")

w("libs/risk-engine/src/risk_engine.cpp", """
#include "mr/risk_engine/risk_engine.hpp"
namespace mr {
SizingResult RiskEngine::size_position(const TradeIntent& intent, double balance, double price) {
    SizingResult r;
    if (emergency_stop) { r.reason = "EMERGENCY_STOP"; return r; }
    if (intent.decision != EntryDecision::EntryReady) { r.reason = "NOT_READY"; return r; }
    double risk_per_unit = std::abs(price - intent.stop_loss);
    if (risk_per_unit <= 0) { r.reason = "INVALID_STOP"; return r; }
    double risk_budget = balance * (limits_.max_drawdown_pct / 100.0);
    r.quantity = std::min(limits_.max_position_size, risk_budget / risk_per_unit);
    r.approved = r.quantity > 0; return r;
}
GuardResult RiskEngine::pre_trade_check(const TradeIntent& intent, double spread_cost) {
    GuardResult g;
    if (emergency_stop) { g.pass = false; g.reason = "EMERGENCY_STOP"; return g; }
    if (daily_pnl_ <= -limits_.max_daily_loss) { g.pass = false; g.reason = "DAILY_LOSS"; return g; }
    if (intent.expected_value <= spread_cost) { g.pass = false; g.reason = "NEGATIVE_EV"; return g; }
    return g;
}
GuardResult RiskEngine::monitor_position(const PositionState& pos, double daily_pnl) {
    daily_pnl_ = daily_pnl;
    GuardResult g;
    if (emergency_stop || daily_pnl <= -limits_.max_daily_loss) { g.pass = false; g.reason = "EMERGENCY"; }
    return g;
}
}""")

w("libs/risk-engine/CMakeLists.txt", """
add_library(mr_risk_engine STATIC src/risk_engine.cpp)
add_library(mr::risk-engine ALIAS mr_risk_engine)
target_include_directories(mr_risk_engine PUBLIC include)
target_link_libraries(mr_risk_engine PUBLIC mr::position-brain mr::decision-engine)
target_compile_features(mr_risk_engine PUBLIC cxx_std_20)
""")

print("decision stack done")
