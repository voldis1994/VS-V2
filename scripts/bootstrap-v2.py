#!/usr/bin/env python3
"""Bootstrap VS-V2 scaffold files."""
from __future__ import annotations
import json
import os
import textwrap

ROOT = "/workspace"


def write(path: str, content: str) -> None:
    full = os.path.join(ROOT, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "w", encoding="utf-8") as f:
        f.write(content if content.endswith("\n") else content + "\n")


# --- V2 engine libs ---

write(
    "libs/candle-engine/include/mr/candle/candle_engine.hpp",
    textwrap.dedent(
        """#pragma once

#include "mr/common/market_event.hpp"
#include "mr/feed_fusion/feed_fusion_engine.hpp"
#include <deque>
#include <unordered_map>
#include <vector>

namespace mr {

struct CandleBar {
    InstrumentId instrument{kInvalidInstrument};
    Timestamp bucket_start{};
    double open{0};
    double high{0};
    double low{0};
    double close{0};
    double volume{0};
    std::uint32_t tick_count{0};
};

class CandleEngine {
public:
    explicit CandleEngine(std::int64_t bucket_ns = 10'000'000'000LL);
    void ingest(InstrumentId instrument, const FeedConsensus& consensus, Timestamp ts);
    [[nodiscard]] std::vector<CandleBar> recent(InstrumentId instrument, std::size_t limit = 64) const;
    [[nodiscard]] CandleBar latest(InstrumentId instrument) const;

private:
    std::int64_t bucket_ns_;
    struct State {
        CandleBar current{};
        std::deque<CandleBar> history;
    };
    std::unordered_map<InstrumentId, State> states_;
    void roll_bucket(State& state, InstrumentId instrument, Timestamp ts);
};

}  // namespace mr
"""
    ),
)

write(
    "libs/candle-engine/src/candle_engine.cpp",
    textwrap.dedent(
        """#include "mr/candle/candle_engine.hpp"

namespace mr {

CandleEngine::CandleEngine(std::int64_t bucket_ns) : bucket_ns_(bucket_ns) {}

void CandleEngine::roll_bucket(State& state, InstrumentId instrument, Timestamp ts) {
    if (state.current.tick_count > 0) {
        state.history.push_back(state.current);
        if (state.history.size() > 256) state.history.pop_front();
    }
    state.current = {};
    state.current.instrument = instrument;
    state.current.bucket_start = ts;
}

void CandleEngine::ingest(InstrumentId instrument, const FeedConsensus& consensus, Timestamp ts) {
    if (consensus.mid_price <= 0) return;
    auto& state = states_[instrument];
    if (state.current.tick_count == 0) {
        state.current.instrument = instrument;
        state.current.bucket_start = ts;
        state.current.open = consensus.mid_price;
        state.current.high = consensus.mid_price;
        state.current.low = consensus.mid_price;
        state.current.close = consensus.mid_price;
        state.current.tick_count = 1;
        return;
    }
    auto elapsed = ts - state.current.bucket_start;
    if (elapsed.count() >= bucket_ns_) {
        roll_bucket(state, instrument, ts);
        state.current.open = consensus.mid_price;
        state.current.high = consensus.mid_price;
        state.current.low = consensus.mid_price;
        state.current.close = consensus.mid_price;
        state.current.tick_count = 1;
        return;
    }
    state.current.high = std::max(state.current.high, consensus.mid_price);
    state.current.low = std::min(state.current.low, consensus.mid_price);
    state.current.close = consensus.mid_price;
    state.current.tick_count++;
}

std::vector<CandleBar> CandleEngine::recent(InstrumentId instrument, std::size_t limit) const {
    auto it = states_.find(instrument);
    if (it == states_.end()) return {};
    std::vector<CandleBar> out;
    const auto& hist = it->second.history;
    std::size_t start = hist.size() > limit ? hist.size() - limit : 0;
    for (std::size_t i = start; i < hist.size(); ++i) out.push_back(hist[i]);
    if (it->second.current.tick_count > 0) out.push_back(it->second.current);
    return out;
}

CandleBar CandleEngine::latest(InstrumentId instrument) const {
    auto it = states_.find(instrument);
    if (it == states_.end() || it->second.current.tick_count == 0) return {};
    return it->second.current;
}

}  // namespace mr
"""
    ),
)

write(
    "libs/candle-engine/CMakeLists.txt",
    """add_library(mr_candle_engine src/candle_engine.cpp)
add_library(mr::candle_engine ALIAS mr_candle_engine)
target_include_directories(mr_candle_engine PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(mr_candle_engine PUBLIC mr::common mr::feed_fusion)
target_compile_features(mr_candle_engine PUBLIC cxx_std_20)
""",
)

write(
    "libs/brain-core/include/mr/brain/brain_engine.hpp",
    textwrap.dedent(
        """#pragma once

#include "mr/candle/candle_engine.hpp"
#include <string>
#include <unordered_map>

namespace mr {

struct BrainScores {
    double structure{0};
    double momentum{0};
    double pressure{0};
    double behavior{0};
    double impact{0};
    double composite{0};
    std::string bias{"NEUTRAL"};
};

struct BrainSnapshot {
    InstrumentId instrument{kInvalidInstrument};
    Timestamp timestamp{};
    BrainScores scores{};
    std::size_t bar_count{0};
};

class BrainEngine {
public:
    BrainSnapshot update(InstrumentId instrument, const std::vector<CandleBar>& bars, Timestamp ts);

private:
    std::unordered_map<InstrumentId, BrainSnapshot> latest_;
};

}  // namespace mr
"""
    ),
)

write(
    "libs/brain-core/src/brain_engine.cpp",
    textwrap.dedent(
        """#include "mr/brain/brain_engine.hpp"
#include <algorithm>
#include <cmath>

namespace mr {

static double clamp(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}

BrainSnapshot BrainEngine::update(InstrumentId instrument, const std::vector<CandleBar>& bars, Timestamp ts) {
    BrainSnapshot snap;
    snap.instrument = instrument;
    snap.timestamp = ts;
    snap.bar_count = bars.size();
    if (bars.empty()) {
        latest_[instrument] = snap;
        return snap;
    }

    double first = bars.front().close;
    double last = bars.back().close;
    double roc = first != 0 ? (last - first) / first : 0.0;
    snap.scores.momentum = clamp(roc * 10.0, -1.0, 1.0);

    double swing_high = bars[0].high;
    double swing_low = bars[0].low;
    for (const auto& b : bars) {
        swing_high = std::max(swing_high, b.high);
        swing_low = std::min(swing_low, b.low);
    }
    snap.scores.structure = last > first ? 0.6 : (last < first ? -0.6 : 0.0);

    double range = swing_high - swing_low;
    snap.scores.pressure = range > 0 ? clamp((last - swing_low) / range * 2.0 - 1.0, -1.0, 1.0) : 0.0;

    double vol = 0;
    for (const auto& b : bars) vol += std::abs(b.close - b.open);
    snap.scores.behavior = clamp(vol / (bars.size() * (range > 0 ? range : 1.0)), 0.0, 1.0);

    snap.scores.impact = clamp(
        0.35 * snap.scores.structure + 0.35 * snap.scores.momentum + 0.2 * snap.scores.pressure + 0.1 * snap.scores.behavior,
        -1.0, 1.0);

    snap.scores.composite = snap.scores.impact;
    if (snap.scores.composite > 0.25) snap.scores.bias = "BULLISH";
    else if (snap.scores.composite < -0.25) snap.scores.bias = "BEARISH";
    else snap.scores.bias = "NEUTRAL";

    latest_[instrument] = snap;
    return snap;
}

}  // namespace mr
"""
    ),
)

write(
    "libs/brain-core/CMakeLists.txt",
    """add_library(mr_brain_core src/brain_engine.cpp)
add_library(mr::brain_core ALIAS mr_brain_core)
target_include_directories(mr_brain_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(mr_brain_core PUBLIC mr::common mr::candle_engine)
target_compile_features(mr_brain_core PUBLIC cxx_std_20)
""",
)

write(
    "libs/decision-engine/include/mr/decision/decision_engine.hpp",
    textwrap.dedent(
        """#pragma once

#include "mr/brain/brain_engine.hpp"
#include <string>
#include <vector>

namespace mr {

enum class DecisionAction : std::uint8_t { Wait = 0, Buy = 1, Sell = 2, Block = 3 };

struct DecisionResult {
    InstrumentId instrument{kInvalidInstrument};
    DecisionAction action{DecisionAction::Wait};
    double buy_score{0};
    double sell_score{0};
    double confidence{0};
    std::string reason;
    std::vector<std::string> reason_codes;
};

class DecisionEngine {
public:
    DecisionResult evaluate(const BrainSnapshot& brain, double spread, double min_confidence = 0.55);
};

}  // namespace mr
"""
    ),
)

write(
    "libs/decision-engine/src/decision_engine.cpp",
    textwrap.dedent(
        """#include "mr/decision/decision_engine.hpp"
#include <cmath>

namespace mr {

DecisionResult DecisionEngine::evaluate(const BrainSnapshot& brain, double spread, double min_confidence) {
    DecisionResult out;
    out.instrument = brain.instrument;
    const auto& s = brain.scores;

    out.buy_score = std::max(0.0, s.composite) * (1.0 - std::min(spread * 10000.0, 0.5));
    out.sell_score = std::max(0.0, -s.composite) * (1.0 - std::min(spread * 10000.0, 0.5));
    out.confidence = std::max(out.buy_score, out.sell_score);

    if (brain.bar_count < 3) {
        out.action = DecisionAction::Wait;
        out.reason = "Insufficient candle history";
        out.reason_codes.push_back("WAIT_BARS");
        return out;
    }
    if (out.confidence < min_confidence) {
        out.action = DecisionAction::Wait;
        out.reason = "Confidence below threshold";
        out.reason_codes.push_back("WAIT_CONFIDENCE");
        return out;
    }
    if (out.buy_score > out.sell_score) {
        out.action = DecisionAction::Buy;
        out.reason = "Brain composite bullish";
        out.reason_codes.push_back("BUY_BRAIN");
    } else if (out.sell_score > out.buy_score) {
        out.action = DecisionAction::Sell;
        out.reason = "Brain composite bearish";
        out.reason_codes.push_back("SELL_BRAIN");
    } else {
        out.action = DecisionAction::Wait;
        out.reason = "Scores tied";
        out.reason_codes.push_back("WAIT_TIE");
    }
    return out;
}

}  // namespace mr
"""
    ),
)

write(
    "libs/decision-engine/CMakeLists.txt",
    """add_library(mr_decision_engine src/decision_engine.cpp)
add_library(mr::decision_engine ALIAS mr_decision_engine)
target_include_directories(mr_decision_engine PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(mr_decision_engine PUBLIC mr::common mr::brain_core)
target_compile_features(mr_decision_engine PUBLIC cxx_std_20)
""",
)

write(
    "libs/risk-engine/include/mr/risk/risk_engine.hpp",
    textwrap.dedent(
        """#pragma once

#include "mr/decision/decision_engine.hpp"
#include <vector>

namespace mr {

enum class RiskIntentType : std::uint8_t { None = 0, Entry = 1, Exit = 2, Reduce = 3 };

struct RiskIntent {
    TradeIntentId id{0};
    InstrumentId instrument{kInvalidInstrument};
    RiskIntentType type{RiskIntentType::None};
    Direction direction{Direction::Flat};
    double reference_price{0};
    double size_fraction{0};
    double max_risk_fraction{0};
    double confidence{0};
    std::string human_explanation;
    std::vector<std::string> reason_codes;
};

class RiskEngine {
public:
    explicit RiskEngine(IdGenerator& intent_ids);
    std::vector<RiskIntent> from_decision(const DecisionResult& decision, double mid_price,
                                          double account_risk_budget = 0.01);

private:
    IdGenerator& intent_ids_;
};

}  // namespace mr
"""
    ),
)

write(
    "libs/risk-engine/src/risk_engine.cpp",
    textwrap.dedent(
        """#include "mr/risk/risk_engine.hpp"

namespace mr {

RiskEngine::RiskEngine(IdGenerator& intent_ids) : intent_ids_(intent_ids) {}

std::vector<RiskIntent> RiskEngine::from_decision(const DecisionResult& decision, double mid_price,
                                                    double account_risk_budget) {
    std::vector<RiskIntent> intents;
    if (decision.action == DecisionAction::Wait || decision.action == DecisionAction::Block) {
        return intents;
    }
    RiskIntent intent;
    intent.id = intent_ids_.generate();
    intent.instrument = decision.instrument;
    intent.type = RiskIntentType::Entry;
    intent.direction = decision.action == DecisionAction::Buy ? Direction::Long : Direction::Short;
    intent.reference_price = mid_price;
    intent.confidence = decision.confidence;
    intent.size_fraction = account_risk_budget * decision.confidence;
    intent.max_risk_fraction = account_risk_budget;
    intent.human_explanation = decision.reason;
    intent.reason_codes = decision.reason_codes;
    intents.push_back(intent);
    return intents;
}

}  // namespace mr
"""
    ),
)

write(
    "libs/risk-engine/CMakeLists.txt",
    """add_library(mr_risk_engine src/risk_engine.cpp)
add_library(mr::risk_engine ALIAS mr_risk_engine)
target_include_directories(mr_risk_engine PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(mr_risk_engine PUBLIC mr::common mr::decision_engine)
target_compile_features(mr_risk_engine PUBLIC cxx_std_20)
""",
)

print("V2 engine libs written")
