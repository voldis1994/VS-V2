#!/usr/bin/env python3
from pathlib import Path
import textwrap
ROOT = Path("/workspace")
def w(p, c):
    path = ROOT / p
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(textwrap.dedent(c).lstrip("\n"))

# candle-engine headers
headers = {
"forming_candle.hpp": """
#pragma once
#include "mr/market_types/candle.hpp"
namespace mr { using FormingCandle = Candle; }""",
"closed_candle.hpp": """
#pragma once
#include "mr/market_types/candle.hpp"
namespace mr {
struct ClosedCandle : Candle { Timestamp close_time{}; };
}""",
"candle_event.hpp": """
#pragma once
#include "mr/market_types/candle.hpp"
namespace mr {
enum class CandleEventType : std::uint8_t { Tick=0, Closed=1, Gap=2 };
struct CandleEvent { CandleEventType type{CandleEventType::Tick}; Candle candle{}; Timestamp ts{}; };
}""",
"candle_series.hpp": """
#pragma once
#include "mr/market_types/candle.hpp"
#include <deque>
namespace mr {
class CandleSeries {
public:
    void push_closed(const Candle& c);
    [[nodiscard]] const std::deque<Candle>& closed() const { return closed_; }
    [[nodiscard]] const Candle* last_closed() const { return closed_.empty() ? nullptr : &closed_.back(); }
private:
    std::deque<Candle> closed_;
};
}""",
"candle_builder.hpp": """
#pragma once
#include "mr/market_types/candle.hpp"
#include "mr/market_types/timeframe.hpp"
namespace mr {
class CandleBuilder {
public:
    explicit CandleBuilder(Timeframe tf) : tf_(tf), bucket_ns_(timeframe_ns(tf)) {}
    virtual ~CandleBuilder() = default;
    virtual CandleEvent on_tick(double price, Timestamp ts, InstrumentId inst);
    [[nodiscard]] const Candle& forming() const { return forming_; }
    [[nodiscard]] bool has_forming() const { return has_forming_; }
protected:
    virtual std::uint64_t bucket_start(Timestamp ts) const;
    Timeframe tf_;
    std::uint64_t bucket_ns_;
    Candle forming_{};
    bool has_forming_{false};
};
}""",
"second_builder.hpp": """
#pragma once
#include "mr/candle_engine/candle_builder.hpp"
namespace mr { class SecondBuilder : public CandleBuilder { public: SecondBuilder() : CandleBuilder(Timeframe::Second1) {} }; }""",
"ten_second_builder.hpp": """
#pragma once
#include "mr/candle_engine/candle_builder.hpp"
namespace mr { class TenSecondBuilder : public CandleBuilder { public: TenSecondBuilder() : CandleBuilder(Timeframe::Second10) {} }; }""",
"timeframe_aggregator.hpp": """
#pragma once
#include "mr/candle_engine/candle_series.hpp"
#include "mr/market_types/timeframe.hpp"
namespace mr {
class TimeframeAggregator {
public:
    explicit TimeframeAggregator(Timeframe target) : target_(target), bucket_ns_(timeframe_ns(target)) {}
    std::optional<Candle> on_closed_candle(const Candle& c);
private:
    Timeframe target_;
    std::uint64_t bucket_ns_;
    Candle forming_{};
    bool has_{false};
};
}""",
"candle_history.hpp": """
#pragma once
#include "mr/candle_engine/candle_series.hpp"
namespace mr {
class CandleHistory {
public:
    void add(const Candle& c);
    [[nodiscard]] std::size_t size() const { return series_.closed().size(); }
    [[nodiscard]] const CandleSeries& series() const { return series_; }
private:
    CandleSeries series_;
};
}""",
"candle_deduplicator.hpp": """
#pragma once
#include "mr/market_types/candle.hpp"
namespace mr {
class CandleDeduplicator {
public:
    bool accept(const Candle& c);
private:
    Timestamp last_open_{};
};
}""",
"candle_gap_handler.hpp": """
#pragma once
#include "mr/market_types/candle.hpp"
#include <vector>
namespace mr {
class CandleGapHandler {
public:
    std::vector<Candle> detect_gaps(const Candle& prev, const Candle& next, std::uint64_t bucket_ns);
};
}""",
"candle_validator.hpp": """
#pragma once
#include "mr/market_types/candle.hpp"
namespace mr {
struct ValidationResult { bool ok{true}; std::string reason; };
class CandleValidator {
public:
    ValidationResult validate(const Candle& c) const;
};
}""",
"candle_engine.hpp": """
#pragma once
#include "mr/candle_engine/ten_second_builder.hpp"
#include "mr/candle_engine/second_builder.hpp"
#include "mr/candle_engine/candle_history.hpp"
#include "mr/candle_engine/candle_deduplicator.hpp"
#include "mr/candle_engine/candle_gap_handler.hpp"
#include "mr/candle_engine/candle_validator.hpp"
#include "mr/market_types/market_event.hpp"
namespace mr {
struct CandleEngineState {
    Candle forming_10s{};
    ClosedCandle last_closed_10s{};
    bool has_forming{false};
    bool has_closed{false};
    double bucket_progress{0};
};
class CandleEngine {
public:
    void on_quote(double price, Timestamp ts, InstrumentId inst);
    void on_event(const NormalizedEvent& e, double consensus_mid);
    [[nodiscard]] CandleEngineState state() const;
    [[nodiscard]] const CandleHistory& history() const { return history_; }
    void reset();
private:
    TenSecondBuilder builder_10s_;
    SecondBuilder builder_1s_;
    CandleHistory history_;
    CandleDeduplicator dedup_;
    CandleGapHandler gaps_;
    CandleValidator validator_;
    CandleEngineState state_{};
    InstrumentId instrument_{kInvalidInstrument};
};
}""",
}

for name, body in headers.items():
    w(f"libs/candle-engine/include/mr/candle_engine/{name}", body)

w("libs/candle-engine/src/candle_builder.cpp", """
#include "mr/candle_engine/candle_builder.hpp"
namespace mr {
std::uint64_t CandleBuilder::bucket_start(Timestamp ts) const {
    auto ns = static_cast<std::uint64_t>(ts.count());
    return (ns / bucket_ns_) * bucket_ns_;
}
CandleEvent CandleBuilder::on_tick(double price, Timestamp ts, InstrumentId inst) {
    CandleEvent ev; ev.ts = ts;
    auto bucket = bucket_start(ts);
    if (!has_forming_ || static_cast<std::uint64_t>(forming_.open_time.count()) != bucket) {
        if (has_forming_ && forming_.ticks > 0) {
            forming_.status = CandleStatus::Closed;
            ev.type = CandleEventType::Closed; ev.candle = forming_;
        }
        forming_ = {}; forming_.instrument = inst;
        forming_.open_time = Timestamp(static_cast<long long>(bucket));
        forming_.open = forming_.high = forming_.low = forming_.close = price;
        forming_.ticks = 1; forming_.status = CandleStatus::Forming;
        has_forming_ = true;
        if (ev.type != CandleEventType::Closed) ev.type = CandleEventType::Tick;
        ev.candle = forming_;
        return ev;
    }
    forming_.high = std::max(forming_.high, price);
    forming_.low = std::min(forming_.low, price);
    forming_.close = price; forming_.ticks++;
    ev.type = CandleEventType::Tick; ev.candle = forming_;
    return ev;
}
}""")

w("libs/candle-engine/src/second_builder.cpp", '#include "mr/candle_engine/second_builder.hpp"\n')
w("libs/candle-engine/src/ten_second_builder.cpp", '#include "mr/candle_engine/ten_second_builder.hpp"\n')

w("libs/candle-engine/src/timeframe_aggregator.cpp", """
#include "mr/candle_engine/timeframe_aggregator.hpp"
namespace mr {
std::optional<Candle> TimeframeAggregator::on_closed_candle(const Candle& c) {
    auto bucket = (static_cast<std::uint64_t>(c.open_time.count()) / bucket_ns_) * bucket_ns_;
    if (!has_ || static_cast<std::uint64_t>(forming_.open_time.count()) != bucket) {
        if (has_ && forming_.ticks > 0) {
            Candle out = forming_; forming_ = c; forming_.open_time = Timestamp(static_cast<long long>(bucket));
            has_ = true; return out;
        }
        forming_ = c; forming_.open_time = Timestamp(static_cast<long long>(bucket)); has_ = true; return std::nullopt;
    }
    forming_.high = std::max(forming_.high, c.high);
    forming_.low = std::min(forming_.low, c.low);
    forming_.close = c.close; forming_.ticks += c.ticks;
    return std::nullopt;
}
}""")

w("libs/candle-engine/src/candle_series.cpp", """
#include "mr/candle_engine/candle_series.hpp"
namespace mr {
void CandleSeries::push_closed(const Candle& c) { closed_.push_back(c); if (closed_.size() > 10000) closed_.pop_front(); }
}""")

w("libs/candle-engine/src/candle_history.cpp", """
#include "mr/candle_engine/candle_history.hpp"
namespace mr {
void CandleHistory::add(const Candle& c) { if (c.status == CandleStatus::Closed) series_.push_closed(c); }
}""")

w("libs/candle-engine/src/candle_deduplicator.cpp", """
#include "mr/candle_engine/candle_deduplicator.hpp"
namespace mr {
bool CandleDeduplicator::accept(const Candle& c) {
    if (c.open_time == last_open_) return false;
    last_open_ = c.open_time; return true;
}
}""")

w("libs/candle-engine/src/candle_gap_handler.cpp", """
#include "mr/candle_engine/candle_gap_handler.hpp"
namespace mr {
std::vector<Candle> CandleGapHandler::detect_gaps(const Candle& prev, const Candle& next, std::uint64_t bucket_ns) {
    std::vector<Candle> gaps;
    auto expected = static_cast<std::uint64_t>(prev.open_time.count()) + bucket_ns;
    auto actual = static_cast<std::uint64_t>(next.open_time.count());
    while (expected < actual) {
        Candle g; g.open_time = Timestamp(static_cast<long long>(expected));
        g.status = CandleStatus::Gap; gaps.push_back(g);
        expected += bucket_ns;
    }
    return gaps;
}
}""")

w("libs/candle-engine/src/candle_validator.cpp", """
#include "mr/candle_engine/candle_validator.hpp"
namespace mr {
ValidationResult CandleValidator::validate(const Candle& c) const {
    ValidationResult r;
    if (c.high < c.low) { r.ok = false; r.reason = "high < low"; return r; }
    if (c.open > c.high || c.open < c.low || c.close > c.high || c.close < c.low) {
        r.ok = false; r.reason = "ohlc out of range"; return r;
    }
    if (c.ticks == 0 && c.status != CandleStatus::Gap) { r.ok = false; r.reason = "no ticks"; }
    return r;
}
}""")

w("libs/candle-engine/src/candle_engine.cpp", """
#include "mr/candle_engine/candle_engine.hpp"
namespace mr {
void CandleEngine::on_quote(double price, Timestamp ts, InstrumentId inst) {
    instrument_ = inst;
    auto ev = builder_10s_.on_tick(price, ts, inst);
    state_.forming_10s = builder_10s_.forming();
    state_.has_forming = builder_10s_.has_forming();
    auto ns = static_cast<std::uint64_t>(ts.count());
    auto bucket = (ns / timeframe_ns(Timeframe::Second10)) * timeframe_ns(Timeframe::Second10);
    state_.bucket_progress = std::clamp(static_cast<double>(ns - bucket) / timeframe_ns(Timeframe::Second10), 0.0, 1.0);
    if (ev.type == CandleEventType::Closed) {
        auto c = ev.candle; c.status = CandleStatus::Closed;
        if (dedup_.accept(c) && validator_.validate(c).ok) {
            ClosedCandle closed; closed = static_cast<ClosedCandle>(c);
            closed.close_time = ts;
            state_.last_closed_10s = closed; state_.has_closed = true;
            history_.add(c);
        }
    }
    builder_1s_.on_tick(price, ts, inst);
}
void CandleEngine::on_event(const NormalizedEvent& e, double consensus_mid) {
    double price = consensus_mid;
    if (price <= 0) {
        if (e.last) price = *e.last;
        else if (e.bid && e.ask) price = (*e.bid + *e.ask) * 0.5;
    }
    if (price > 0) on_quote(price, e.normalized_timestamp, e.instrument);
}
CandleEngineState CandleEngine::state() const { return state_; }
void CandleEngine::reset() { state_ = {}; history_ = CandleHistory{}; }
}""")

# Tests
w("libs/candle-engine/tests/test_second.cpp", """
#include <gtest/gtest.h>
#include "mr/candle_engine/second_builder.hpp"
using namespace mr;
TEST(SecondBuilder, FormsBar) {
    SecondBuilder b;
    auto ev = b.on_tick(100.0, Timestamp(0), 1);
    EXPECT_EQ(ev.candle.ticks, 1u);
    ev = b.on_tick(101.0, Timestamp(1'500'000'000LL), 1);
    EXPECT_EQ(ev.candle.close, 101.0);
}
""")

w("libs/candle-engine/tests/test_ten_second.cpp", """
#include <gtest/gtest.h>
#include "mr/candle_engine/ten_second_builder.hpp"
using namespace mr;
TEST(TenSecondBuilder, ClosesOnBoundary) {
    TenSecondBuilder b;
    for (int i = 0; i < 5; ++i) b.on_tick(100.0 + i, Timestamp(static_cast<long long>(i) * 1'000'000'000LL), 1);
    auto ev = b.on_tick(105.0, Timestamp(10'000'000'000LL), 1);
    EXPECT_EQ(ev.type, CandleEventType::Closed);
    EXPECT_DOUBLE_EQ(ev.candle.open, 100.0);
}
""")

w("libs/candle-engine/tests/test_boundary.cpp", """
#include <gtest/gtest.h>
#include "mr/candle_engine/candle_engine.hpp"
using namespace mr;
TEST(CandleEngine, BoundaryProgress) {
    CandleEngine eng;
    eng.on_quote(100.0, Timestamp(5'000'000'000LL), 1);
    auto st = eng.state();
    EXPECT_NEAR(st.bucket_progress, 0.5, 0.01);
}
""")

w("libs/candle-engine/tests/test_gap.cpp", """
#include <gtest/gtest.h>
#include "mr/candle_engine/candle_gap_handler.hpp"
using namespace mr;
TEST(GapHandler, DetectsMissingBuckets) {
    CandleGapHandler h;
    Candle a, b; a.open_time = Timestamp(0); b.open_time = Timestamp(30'000'000'000LL);
    auto gaps = h.detect_gaps(a, b, timeframe_ns(Timeframe::Second10));
    EXPECT_EQ(gaps.size(), 2u);
}
""")

w("libs/candle-engine/tests/test_out_of_order.cpp", """
#include <gtest/gtest.h>
#include "mr/candle_engine/candle_deduplicator.hpp"
using namespace mr;
TEST(Deduplicator, RejectsDuplicate) {
    CandleDeduplicator d;
    Candle c; c.open_time = Timestamp(0);
    EXPECT_TRUE(d.accept(c));
    EXPECT_FALSE(d.accept(c));
}
""")

w("libs/candle-engine/tests/test_close_once.cpp", """
#include <gtest/gtest.h>
#include "mr/candle_engine/candle_engine.hpp"
using namespace mr;
TEST(CandleEngine, CloseOncePerBucket) {
    CandleEngine eng;
    for (int i = 0; i < 5; ++i)
        eng.on_quote(100.0 + i * 0.1, Timestamp(static_cast<long long>(i) * 1'000'000'000LL), 1);
    EXPECT_FALSE(eng.state().has_closed);
    eng.on_quote(101.0, Timestamp(10'000'000'000LL), 1);
    EXPECT_TRUE(eng.state().has_closed);
    EXPECT_TRUE(eng.state().last_closed_10s.status == CandleStatus::Closed);
    auto closed_open = eng.state().last_closed_10s.open_time;
    eng.on_quote(102.0, Timestamp(11'000'000'000LL), 1);
    EXPECT_EQ(eng.state().last_closed_10s.open_time, closed_open);
}
""")

w("libs/candle-engine/CMakeLists.txt", """
add_library(mr_candle_engine STATIC
    src/candle_engine.cpp src/candle_builder.cpp src/second_builder.cpp src/ten_second_builder.cpp
    src/timeframe_aggregator.cpp src/candle_series.cpp src/candle_history.cpp
    src/candle_deduplicator.cpp src/candle_gap_handler.cpp src/candle_validator.cpp)
add_library(mr::candle-engine ALIAS mr_candle_engine)
target_include_directories(mr_candle_engine PUBLIC include)
target_link_libraries(mr_candle_engine PUBLIC mr::common mr::market-types)
target_compile_features(mr_candle_engine PUBLIC cxx_std_20)

if(MR_BUILD_TESTS AND TARGET GTest::gtest_main)
    add_executable(mr_candle_engine_tests
        tests/test_second.cpp tests/test_ten_second.cpp tests/test_boundary.cpp
        tests/test_gap.cpp tests/test_out_of_order.cpp tests/test_close_once.cpp)
    target_link_libraries(mr_candle_engine_tests PRIVATE mr::candle-engine GTest::gtest_main)
    add_test(NAME candle_engine COMMAND mr_candle_engine_tests)
endif()
""")

print("candle-engine done")
