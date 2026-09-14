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
}