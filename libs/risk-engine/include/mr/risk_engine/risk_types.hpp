#pragma once
namespace mr {
struct RiskLimits { double max_position_size{1.0}; double max_daily_loss{1000}; double max_drawdown_pct{5}; };
struct SizingResult { double quantity{0}; bool approved{false}; std::string reason; };
struct GuardResult { bool pass{true}; std::string reason; };
}