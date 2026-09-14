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
