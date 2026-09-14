#pragma once
#include <cstdint>
namespace mr {
enum class Timeframe : std::uint32_t { Second1=1, Second10=10, Minute1=60, Minute5=300, Minute15=900, Hour1=3600 };
inline std::uint64_t timeframe_ns(Timeframe tf) {
    switch(tf) {
        case Timeframe::Second1: return 1'000'000'000ULL;
        case Timeframe::Second10: return 10'000'000'000ULL;
        case Timeframe::Minute1: return 60'000'000'000ULL;
        case Timeframe::Minute5: return 300'000'000'000ULL;
        case Timeframe::Minute15: return 900'000'000'000ULL;
        case Timeframe::Hour1: return 3'600'000'000'000ULL;
    }
    return 1'000'000'000ULL;
}
}