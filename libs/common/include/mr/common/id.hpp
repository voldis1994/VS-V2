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
enum class OperatingMode : std::uint8_t { Replay = 0, Paper = 1, Demo = 2, Live = 3, Shadow = 4 };
enum class HealthStatus : std::uint8_t { Healthy = 0, Degraded = 1, Unhealthy = 2, Disconnected = 3 };
enum class ErrorSeverity : std::uint8_t { Recoverable = 0, Degraded = 1, Critical = 2 };

struct IdGenerator {
    std::uint64_t next{1};
    std::uint64_t generate() { return next++; }
};

}  // namespace mr
