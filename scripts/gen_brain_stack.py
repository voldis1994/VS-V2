#!/usr/bin/env python3
from pathlib import Path
import textwrap, shutil
ROOT = Path("/workspace")
def w(p, c):
    path = ROOT / p
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(textwrap.dedent(c).lstrip("\n"))

# ── brain-core ──────────────────────────────────────────────────────────────
for h in ["brain_event_type.hpp", "brain_version.hpp"]:
    pass

w("libs/brain-core/include/mr/brain_core/brain_version.hpp", """
#pragma once
namespace mr { inline constexpr const char* kBrainVersion = "vs-v2-1.0.0"; }""")

w("libs/brain-core/include/mr/brain_core/brain_event_type.hpp", """
#pragma once
#include <cstdint>
namespace mr {
enum class BrainEventType : std::uint8_t {
    Quote=0, CandleClosed=1, Perception=2, Structure=3, Scenario=4, Decision=5, Position=6
};
}""")

w("libs/brain-core/include/mr/brain_core/brain_event.hpp", """
#pragma once
#include "mr/brain_core/brain_event_type.hpp"
#include "mr/common/id.hpp"
#include <string>
namespace mr {
struct BrainEvent {
    BrainEventType type{BrainEventType::Quote};
    Timestamp ts{};
    InstrumentId instrument{kInvalidInstrument};
    std::string payload;
};
}""")

w("libs/brain-core/include/mr/brain_core/brain_clock.hpp", """
#pragma once
#include "mr/common/clock.hpp"
namespace mr { using BrainClock = Clock; }""")

w("libs/brain-core/include/mr/brain_core/brain_context.hpp", """
#pragma once
#include "mr/candle_engine/candle_engine.hpp"
#include "mr/feed_fusion/consensus_quote.hpp"
namespace mr {
struct BrainContext {
    InstrumentId instrument{kInvalidInstrument};
    ConsensusQuote consensus{};
    CandleEngineState candles{};
    Timestamp ts{};
};
}""")

w("libs/brain-core/include/mr/brain_core/brain_snapshot.hpp", """
#pragma once
#include "mr/brain_core/brain_context.hpp"
#include <unordered_map>
namespace mr {
struct BrainSnapshot {
    SnapshotId id{0};
    Timestamp ts{};
    std::unordered_map<InstrumentId, BrainContext> instruments;
};
}""")

w("libs/brain-core/include/mr/brain_core/brain_state.hpp", """
#pragma once
#include "mr/brain_core/brain_snapshot.hpp"
namespace mr {
class BrainState {
public:
    void update(const BrainContext& ctx);
    [[nodiscard]] const BrainSnapshot& latest() const { return snapshot_; }
private:
    BrainSnapshot snapshot_;
};
}""")

w("libs/brain-core/include/mr/brain_core/brain_event_router.hpp", """
#pragma once
#include "mr/brain_core/brain_event.hpp"
#include <functional>
#include <vector>
namespace mr {
using BrainEventHandler = std::function<void(const BrainEvent&)>;
class BrainEventRouter {
public:
    void subscribe(BrainEventType type, BrainEventHandler h);
    void publish(const BrainEvent& e);
private:
    std::unordered_map<BrainEventType, std::vector<BrainEventHandler>> handlers_;
};
}""")

w("libs/brain-core/include/mr/brain_core/market_brain.hpp", """
#pragma once
#include "mr/brain_core/brain_state.hpp"
#include "mr/brain_core/brain_event_router.hpp"
#include "mr/candle_engine/candle_engine.hpp"
#include "mr/feed_fusion/feed_fusion_engine.hpp"
#include "mr/market_types/market_event.hpp"
namespace mr {
class MarketBrain {
public:
    void on_normalized(const NormalizedEvent& e, const ConsensusQuote& consensus);
    [[nodiscard]] BrainSnapshot snapshot() const;
    [[nodiscard]] CandleEngine& candles(InstrumentId inst);
    BrainEventRouter& router() { return router_; }
private:
    BrainState state_;
    BrainEventRouter router_;
    std::unordered_map<InstrumentId, CandleEngine> candle_engines_;
    IdGenerator snapshot_ids_;
};
}""")

w("libs/brain-core/src/brain_state.cpp", """
#include "mr/brain_core/brain_state.hpp"
namespace mr {
void BrainState::update(const BrainContext& ctx) {
    snapshot_.instruments[ctx.instrument] = ctx;
    snapshot_.ts = ctx.ts;
}
}""")

w("libs/brain-core/src/brain_event_router.cpp", """
#include "mr/brain_core/brain_event_router.hpp"
namespace mr {
void BrainEventRouter::subscribe(BrainEventType type, BrainEventHandler h) { handlers_[type].push_back(std::move(h)); }
void BrainEventRouter::publish(const BrainEvent& e) {
    auto it = handlers_.find(e.type);
    if (it == handlers_.end()) return;
    for (auto& h : it->second) if (h) h(e);
}
}""")

w("libs/brain-core/src/market_brain.cpp", """
#include "mr/brain_core/market_brain.hpp"
namespace mr {
CandleEngine& MarketBrain::candles(InstrumentId inst) { return candle_engines_[inst]; }
void MarketBrain::on_normalized(const NormalizedEvent& e, const ConsensusQuote& consensus) {
    auto& ce = candles(e.instrument);
    ce.on_event(e, consensus.mid);
    BrainContext ctx; ctx.instrument = e.instrument; ctx.consensus = consensus;
    ctx.candles = ce.state(); ctx.ts = e.normalized_timestamp;
    state_.update(ctx);
    BrainEvent ev; ev.type = BrainEventType::Quote; ev.ts = e.normalized_timestamp;
    ev.instrument = e.instrument; router_.publish(ev);
}
BrainSnapshot MarketBrain::snapshot() const {
    auto s = state_.latest(); s.id = snapshot_ids_.next; return s;
}
}""")

w("libs/brain-core/CMakeLists.txt", """
add_library(mr_brain_core STATIC src/brain_state.cpp src/brain_event_router.cpp src/market_brain.cpp)
add_library(mr::brain-core ALIAS mr_brain_core)
target_include_directories(mr_brain_core PUBLIC include)
target_link_libraries(mr_brain_core PUBLIC mr::common mr::market-types mr::candle-engine mr::feed-fusion)
target_compile_features(mr_brain_core PUBLIC cxx_std_20)
""")

# perception-engine
w("libs/perception-engine/include/mr/perception_engine/price_dynamics.hpp", """
#pragma once
#include "mr/common/rolling_window.hpp"
#include "mr/common/id.hpp"
namespace mr {
struct PriceDynamics {
    double return_value{0}, normalized_return{0}, velocity{0}, acceleration{0};
    double directional_persistence{0}, short_horizon_momentum{0}, displacement{0};
};
struct WindowSample { Timestamp ts{}; double price{0}; };
class PerceptionEngine {
public:
    void update(double price, Timestamp ts);
    [[nodiscard]] PriceDynamics snapshot() const { return current_; }
    void reset();
private:
    RollingWindow<WindowSample, 4096> samples_;
    PriceDynamics current_;
    double prev_price_{0}, prev_velocity_{0}, session_high_{0}, session_low_{0};
};
}""")

w("libs/perception-engine/src/price_dynamics.cpp", """
#include "mr/perception_engine/price_dynamics.hpp"
#include <algorithm>
namespace mr {
void PerceptionEngine::update(double price, Timestamp ts) {
    samples_.push({ts, price});
    if (prev_price_ > 0) {
        current_.return_value = price - prev_price_;
        current_.normalized_return = current_.return_value / prev_price_;
        current_.velocity = current_.return_value;
        current_.acceleration = current_.velocity - prev_velocity_;
        prev_velocity_ = current_.velocity;
        if (current_.return_value > 0) current_.directional_persistence = current_.directional_persistence * 0.9 + 1.0;
        else if (current_.return_value < 0) current_.directional_persistence = current_.directional_persistence * 0.9 - 1.0;
        else current_.directional_persistence *= 0.9;
    }
    prev_price_ = price;
    if (session_high_ == 0 || price > session_high_) session_high_ = price;
    if (session_low_ == 0 || price < session_low_) session_low_ = price;
    if (session_high_ > session_low_) current_.displacement = (price - session_low_) / (session_high_ - session_low_);
    if (samples_.size() >= 2) {
        double oldest = samples_.at(samples_.size() - 1).price;
        double newest = samples_.newest().price;
        current_.short_horizon_momentum = newest - oldest;
    }
}
void PerceptionEngine::reset() { samples_.clear(); current_ = {}; prev_price_ = prev_velocity_ = session_high_ = session_low_ = 0; }
}""")

w("libs/perception-engine/include/mr/perception_engine/perception_engine.hpp", """
#pragma once
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/market_types/market_event.hpp"
namespace mr {
class PerceptionEngineFacade {
public:
    void on_event(const NormalizedEvent& e, double mid);
    [[nodiscard]] PriceDynamics dynamics() const { return engine_.snapshot(); }
private:
    PerceptionEngine engine_;
};
}""")

w("libs/perception-engine/src/perception_engine.cpp", """
#include "mr/perception_engine/perception_engine.hpp"
namespace mr {
void PerceptionEngineFacade::on_event(const NormalizedEvent& e, double mid) {
    double price = mid;
    if (price <= 0) {
        if (e.last) price = *e.last;
        else if (e.bid && e.ask) price = (*e.bid + *e.ask) * 0.5;
    }
    if (price > 0) engine_.update(price, e.normalized_timestamp);
}
}""")

w("libs/perception-engine/CMakeLists.txt", """
add_library(mr_perception_engine STATIC src/price_dynamics.cpp src/perception_engine.cpp)
add_library(mr::perception-engine ALIAS mr_perception_engine)
target_include_directories(mr_perception_engine PUBLIC include)
target_link_libraries(mr_perception_engine PUBLIC mr::common mr::market-types)
target_compile_features(mr_perception_engine PUBLIC cxx_std_20)
""")

# structure-engine
w("libs/structure-engine/include/mr/structure_engine/structure_features.hpp", """
#pragma once
namespace mr {
struct StructureFeatures {
    double swing_state{0}, range_width{0}, range_boundary_distance{0};
    double breakout_strength{0}, failed_breakout{0}, acceptance{0}, rejection{0};
    double pullback_depth{0}, continuation_pressure{0}, compression{0}, expansion{0};
    double reversal_candidate{0}, range_position{0.5};
};
}""")

w("libs/structure-engine/include/mr/structure_engine/structure_engine.hpp", """
#pragma once
#include "mr/structure_engine/structure_features.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/candle_engine/candle_engine.hpp"
namespace mr {
class StructureEngine {
public:
    void update(double price, const PriceDynamics& pd, const CandleEngineState& candles);
    [[nodiscard]] StructureFeatures snapshot() const { return features_; }
private:
    StructureFeatures features_;
    double session_high_{0}, session_low_{0};
};
}""")

w("libs/structure-engine/src/structure_engine.cpp", """
#include "mr/structure_engine/structure_engine.hpp"
#include <algorithm>
namespace mr {
void StructureEngine::update(double price, const PriceDynamics& pd, const CandleEngineState& candles) {
    if (session_high_ == 0 || price > session_high_) session_high_ = price;
    if (session_low_ == 0 || price < session_low_) session_low_ = price;
    features_.range_width = session_high_ - session_low_;
    features_.range_boundary_distance = std::min(price - session_low_, session_high_ - price);
    if (features_.range_width > 0) features_.range_position = (price - session_low_) / features_.range_width;
    features_.swing_state = pd.velocity > 0 ? 1 : (pd.velocity < 0 ? -1 : 0);
    if (candles.has_closed) {
        double bp = candles.last_closed_10s.body_pct();
        if (bp > 0.0004) { features_.continuation_pressure = std::min(1.0, std::abs(bp) * 200); features_.acceptance = features_.continuation_pressure; }
        else if (bp < -0.0004) { features_.rejection = std::min(1.0, std::abs(bp) * 200); }
        if (pd.short_horizon_momentum > 0 && bp < 0) features_.pullback_depth = std::min(1.0, std::abs(bp) * 250);
        else if (pd.short_horizon_momentum < 0 && bp > 0) features_.pullback_depth = std::min(1.0, std::abs(bp) * 250);
        features_.breakout_strength = std::abs(bp) > 0.001 ? std::min(1.0, std::abs(bp) * 100) : 0;
    }
    features_.reversal_candidate = std::abs(pd.directional_persistence) < 0.2 ? 0.5 : 0;
}
}""")

w("libs/structure-engine/CMakeLists.txt", """
add_library(mr_structure_engine STATIC src/structure_engine.cpp)
add_library(mr::structure-engine ALIAS mr_structure_engine)
target_include_directories(mr_structure_engine PUBLIC include)
target_link_libraries(mr_structure_engine PUBLIC mr::perception-engine mr::candle-engine)
target_compile_features(mr_structure_engine PUBLIC cxx_std_20)
""")

# market-concepts
w("libs/market-concepts/include/mr/market_concepts/regime_features.hpp", """
#pragma once
namespace mr {
enum class Regime : std::uint8_t { Unknown=0, Range=1, TrendUp=2, TrendDown=3, Volatile=4 };
struct RegimeFeatures { Regime current{Regime::Unknown}; double confidence{0}; double volatility{0}; double trend_strength{0}; };
inline const char* regime_name(Regime r) {
    switch(r) {
        case Regime::Range: return "RANGE"; case Regime::TrendUp: return "TREND_UP";
        case Regime::TrendDown: return "TREND_DOWN"; case Regime::Volatile: return "VOLATILE"; default: return "UNKNOWN";
    }
}
}""")

w("libs/market-concepts/include/mr/market_concepts/regime_similarity.hpp", """
#pragma once
#include "mr/market_concepts/regime_features.hpp"
namespace mr {
class RegimeSimilarity {
public:
    [[nodiscard]] double compare(const RegimeFeatures& a, const RegimeFeatures& b) const;
};
}""")

w("libs/market-concepts/include/mr/market_concepts/market_concepts_engine.hpp", """
#pragma once
#include "mr/market_concepts/regime_features.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/structure_engine/structure_features.hpp"
namespace mr {
class MarketConceptsEngine {
public:
    RegimeFeatures evaluate(const PriceDynamics& pd, const StructureFeatures& st);
private:
    RegimeSimilarity similarity_;
};
}""")

w("libs/market-concepts/src/regime_similarity.cpp", """
#include "mr/market_concepts/regime_similarity.hpp"
#include <cmath>
namespace mr {
double RegimeSimilarity::compare(const RegimeFeatures& a, const RegimeFeatures& b) const {
    if (a.current != b.current) return 0.2;
    return 1.0 - std::abs(a.confidence - b.confidence) - std::abs(a.trend_strength - b.trend_strength) * 0.5;
}
}""")

w("libs/market-concepts/src/market_concepts_engine.cpp", """
#include "mr/market_concepts/market_concepts_engine.hpp"
#include <cmath>
namespace mr {
RegimeFeatures MarketConceptsEngine::evaluate(const PriceDynamics& pd, const StructureFeatures& st) {
    RegimeFeatures r;
    r.volatility = std::abs(pd.acceleration);
    r.trend_strength = std::abs(pd.directional_persistence);
    if (r.volatility > 0.5) { r.current = Regime::Volatile; r.confidence = 0.7; }
    else if (pd.directional_persistence > 0.5) { r.current = Regime::TrendUp; r.confidence = std::min(1.0, pd.directional_persistence); }
    else if (pd.directional_persistence < -0.5) { r.current = Regime::TrendDown; r.confidence = std::min(1.0, std::abs(pd.directional_persistence)); }
    else { r.current = Regime::Range; r.confidence = 1.0 - st.breakout_strength; }
    return r;
}
}""")

w("libs/market-concepts/CMakeLists.txt", """
add_library(mr_market_concepts STATIC src/regime_similarity.cpp src/market_concepts_engine.cpp)
add_library(mr::market-concepts ALIAS mr_market_concepts)
target_include_directories(mr_market_concepts PUBLIC include)
target_link_libraries(mr_market_concepts PUBLIC mr::perception-engine mr::structure-engine)
target_compile_features(mr_market_concepts PUBLIC cxx_std_20)
""")

# microstructure-engine
w("libs/microstructure-engine/include/mr/microstructure_engine/microstructure_features.hpp", """
#pragma once
namespace mr {
struct MicrostructureFeatures {
    double spread{0}, microprice{0}, bid_ask_imbalance{0};
    double aggressive_buy_pressure{0}, aggressive_sell_pressure{0};
    double exhaustion_proxy{0}, rejection_proxy{0}, reclaim_proxy{0};
};
}""")

w("libs/microstructure-engine/include/mr/microstructure_engine/microstructure_engine.hpp", """
#pragma once
#include "mr/microstructure_engine/microstructure_features.hpp"
#include "mr/market_types/market_event.hpp"
namespace mr {
class MicrostructureEngine {
public:
    void update(const NormalizedEvent& e);
    [[nodiscard]] MicrostructureFeatures snapshot() const { return f_; }
private:
    MicrostructureFeatures f_;
    std::uint64_t trade_count_{0}, quote_count_{0};
};
}""")

w("libs/microstructure-engine/src/microstructure_engine.cpp", """
#include "mr/microstructure_engine/microstructure_engine.hpp"
namespace mr {
void MicrostructureEngine::update(const NormalizedEvent& e) {
    if (e.bid && e.ask) {
        f_.spread = *e.ask - *e.bid;
        double bs = e.bid_size.value_or(1), as = e.ask_size.value_or(1);
        f_.microprice = (*e.bid * as + *e.ask * bs) / (bs + as);
        f_.bid_ask_imbalance = (bs - as) / (bs + as);
        if (e.last) {
            double mid = (*e.bid + *e.ask) * 0.5;
            f_.rejection_proxy = *e.last < mid ? f_.rejection_proxy * 0.8 + 0.2 : f_.rejection_proxy * 0.8;
            f_.reclaim_proxy = *e.last > mid ? f_.reclaim_proxy * 0.8 + 0.2 : f_.reclaim_proxy * 0.8;
        }
    }
    if (e.type == MarketEventType::Trade) {
        trade_count_++;
        if (e.trade_size && e.last && e.bid && e.ask) {
            double mid = (*e.bid + *e.ask) * 0.5;
            if (*e.last >= mid) f_.aggressive_buy_pressure += *e.trade_size;
            else f_.aggressive_sell_pressure += *e.trade_size;
        }
    } else quote_count_++;
    auto total = trade_count_ + quote_count_;
    if (total > 100 && f_.aggressive_buy_pressure + f_.aggressive_sell_pressure > 0) {
        double ratio = f_.aggressive_buy_pressure / (f_.aggressive_buy_pressure + f_.aggressive_sell_pressure);
        f_.exhaustion_proxy = ratio > 0.8 || ratio < 0.2 ? 0.7 : 0;
    }
}
}""")

w("libs/microstructure-engine/CMakeLists.txt", """
add_library(mr_microstructure_engine STATIC src/microstructure_engine.cpp)
add_library(mr::microstructure-engine ALIAS mr_microstructure_engine)
target_include_directories(mr_microstructure_engine PUBLIC include)
target_link_libraries(mr_microstructure_engine PUBLIC mr::market-types mr::perception-engine)
target_compile_features(mr_microstructure_engine PUBLIC cxx_std_20)
""")

# cross-market-engine
w("libs/cross-market-engine/include/mr/cross_market_engine/cross_market_features.hpp", """
#pragma once
namespace mr {
struct CrossMarketFeatures { double correlation{0}, lead_lag{0}, divergence{0}, consensus{0}; };
}""")

w("libs/cross-market-engine/include/mr/cross_market_engine/cross_market_engine.hpp", """
#pragma once
#include "mr/cross_market_engine/cross_market_features.hpp"
#include "mr/feed_fusion/feed_fusion_engine.hpp"
namespace mr {
class CrossMarketEngine {
public:
    void update(InstrumentId inst, const FeedFusionEngine& fusion);
    [[nodiscard]] CrossMarketFeatures features(InstrumentId inst) const;
private:
    std::unordered_map<InstrumentId, CrossMarketFeatures> cache_;
};
}""")

w("libs/cross-market-engine/src/cross_market_engine.cpp", """
#include "mr/cross_market_engine/cross_market_engine.hpp"
namespace mr {
void CrossMarketEngine::update(InstrumentId inst, const FeedFusionEngine& fusion) {
    CrossMarketFeatures f;
    auto c = fusion.consensus(inst); auto d = fusion.divergence(inst); auto ll = fusion.lead_lag(inst);
    f.consensus = c.confidence; f.divergence = d.mean; f.lead_lag = ll.probability;
    f.correlation = 1.0 - std::min(1.0, d.mean / std::max(c.mid, 1e-9) * 1000);
    cache_[inst] = f;
}
CrossMarketFeatures CrossMarketEngine::features(InstrumentId inst) const {
    auto it = cache_.find(inst); return it == cache_.end() ? CrossMarketFeatures{} : it->second;
}
}""")

w("libs/cross-market-engine/CMakeLists.txt", """
add_library(mr_cross_market_engine STATIC src/cross_market_engine.cpp)
add_library(mr::cross-market-engine ALIAS mr_cross_market_engine)
target_include_directories(mr_cross_market_engine PUBLIC include)
target_link_libraries(mr_cross_market_engine PUBLIC mr::feed-fusion)
target_compile_features(mr_cross_market_engine PUBLIC cxx_std_20)
""")

print("brain stack part 1 done")
