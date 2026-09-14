#!/usr/bin/env python3
"""Generate VS-V2 C++ library layer under /workspace/libs."""
from pathlib import Path
import textwrap

ROOT = Path("/workspace")

def w(rel: str, content: str):
    p = ROOT / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(textwrap.dedent(content).lstrip("\n"))
    return p

# ── common ──────────────────────────────────────────────────────────────────
w("libs/common/include/mr/common/id.hpp", """
#pragma once
#include <cstdint>
#include <chrono>

namespace mr {

using InstrumentId = std::uint32_t;
using SourceId = std::uint32_t;
using SetupId = std::uint64_t;
using EvidenceReportId = std::uint64_t;
using TradeIntentId = std::uint64_t;
using ExecutionId = std::uint64_t;
using PositionId = std::uint64_t;
using TradeId = std::uint64_t;
using SnapshotId = std::uint64_t;
using ClientId = std::uint32_t;
using AccountId = std::uint32_t;
using SequenceNumber = std::uint64_t;

constexpr InstrumentId kInvalidInstrument = 0;
constexpr SourceId kInvalidSource = 0;

using Timestamp = std::chrono::nanoseconds;
using SteadyTimestamp = std::chrono::nanoseconds;

enum class Direction : std::uint8_t { Flat = 0, Long = 1, Short = 2 };
enum class OperatingMode : std::uint8_t { Replay = 0, Paper = 1, Demo = 2, Live = 3 };
enum class HealthStatus : std::uint8_t { Healthy = 0, Degraded = 1, Unhealthy = 2, Disconnected = 3 };
enum class ErrorSeverity : std::uint8_t { Recoverable = 0, Degraded = 1, Critical = 2 };

struct IdGenerator {
    std::uint64_t next{1};
    std::uint64_t generate() { return next++; }
};

}  // namespace mr
""")

w("libs/common/include/mr/common/result.hpp", """
#pragma once
#include <string>
#include <variant>
#include <utility>

namespace mr {

template <typename T>
struct Result {
    bool ok{false};
    T value{};
    std::string error;

    static Result success(T v) { return Result{true, std::move(v), {}}; }
    static Result failure(std::string msg) { return Result{false, {}, std::move(msg)}; }
    [[nodiscard]] explicit operator bool() const { return ok; }
};

template <>
struct Result<void> {
    bool ok{false};
    std::string error;
    static Result success() { return Result{true, {}}; }
    static Result failure(std::string msg) { return Result{false, std::move(msg)}; }
    [[nodiscard]] explicit operator bool() const { return ok; }
};

}  // namespace mr
""")

w("libs/common/include/mr/common/clock.hpp", """
#pragma once
#include "mr/common/id.hpp"
#include <chrono>
#include <cstdint>

namespace mr {

inline Timestamp now_utc_ns() {
    return std::chrono::duration_cast<Timestamp>(
        std::chrono::system_clock::now().time_since_epoch());
}

inline SteadyTimestamp now_steady_ns() {
    return std::chrono::duration_cast<SteadyTimestamp>(
        std::chrono::steady_clock::now().time_since_epoch());
}

class Clock {
public:
    virtual ~Clock() = default;
    [[nodiscard]] virtual Timestamp utc_now() const { return now_utc_ns(); }
    [[nodiscard]] virtual SteadyTimestamp steady_now() const { return now_steady_ns(); }
};

class SystemClock final : public Clock {};

class SimulatedClock final : public Clock {
public:
    void set(Timestamp ts) { current_ = ts; }
    void advance(std::int64_t ns) { current_ += std::chrono::nanoseconds(ns); }
    [[nodiscard]] Timestamp utc_now() const override { return current_; }
    [[nodiscard]] SteadyTimestamp steady_now() const override {
        return SteadyTimestamp(current_.count());
    }
private:
    Timestamp current_{};
};

}  // namespace mr
""")

w("libs/common/include/mr/common/math.hpp", """
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

namespace mr {

inline double clamp(double v, double lo, double hi) {
    return std::max(lo, std::min(v, hi));
}

inline double safe_div(double a, double b, double fallback = 0.0) {
    return std::abs(b) > std::numeric_limits<double>::epsilon() ? a / b : fallback;
}

inline double sigmoid(double x) { return 1.0 / (1.0 + std::exp(-x)); }

inline double pct_change(double from, double to) {
    return safe_div(to - from, from);
}

}  // namespace mr
""")

w("libs/common/include/mr/common/statistics.hpp", """
#pragma once
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace mr {

inline double mean(const std::vector<double>& v) {
    if (v.empty()) return 0;
    return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
}

inline double stddev(const std::vector<double>& v) {
    if (v.size() < 2) return 0;
    double m = mean(v);
    double sq = 0;
    for (double x : v) sq += (x - m) * (x - m);
    return std::sqrt(sq / static_cast<double>(v.size()));
}

inline double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    auto idx = static_cast<std::size_t>(p * (v.size() - 1));
    return v[idx];
}

inline double zscore(double x, double m, double s) {
    return s > 1e-12 ? (x - m) / s : 0;
}

}  // namespace mr
""")

w("libs/common/include/mr/common/rolling_window.hpp", """
#pragma once
#include "mr/common/ring_buffer.hpp"
#include <cstddef>
#include <vector>

namespace mr {

template <typename T, std::size_t Capacity>
class RollingWindow {
public:
    void push(const T& v) { buf_.push(v); }
    void push(T&& v) { buf_.push(std::move(v)); }
    [[nodiscard]] std::size_t size() const { return buf_.size(); }
    [[nodiscard]] bool empty() const { return buf_.empty(); }
    [[nodiscard]] const T& newest() const { return buf_.newest(); }
    [[nodiscard]] const T& at(std::size_t age) const { return buf_.at(age); }
    void clear() { buf_.clear(); }

    template <typename Fn>
    [[nodiscard]] std::vector<double> map_values(Fn fn) const {
        std::vector<double> out;
        for (std::size_t i = 0; i < size(); ++i) out.push_back(fn(at(i)));
        return out;
    }

private:
    RingBuffer<T, Capacity> buf_;
};

}  // namespace mr
""")

# Fix ring_buffer include
rb = (ROOT / "libs/common/include/mr/common/ring_buffer.hpp").read_text()
rb = rb.replace('#include "mr/common/types.hpp"', '#include "mr/common/id.hpp"')
(ROOT / "libs/common/include/mr/common/ring_buffer.hpp").write_text(rb)

w("libs/common/CMakeLists.txt", """
add_library(mr_common STATIC
    src/config.cpp
)
add_library(mr::common ALIAS mr_common)
target_include_directories(mr_common PUBLIC include)
target_compile_features(mr_common PUBLIC cxx_std_20)
target_link_libraries(mr_common PUBLIC fmt::fmt yaml-cpp::yaml-cpp)
""")

print("common done")
