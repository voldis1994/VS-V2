#pragma once
namespace mr {
enum class CandleStatus : std::uint8_t { Forming=0, Closed=1, Gap=2, Invalid=3 };
}