#pragma once
#include "mr/common/id.hpp"
namespace mr {
inline std::int64_t to_ms(Timestamp ts) { return ts.count() / 1'000'000; }
inline Timestamp from_ms(std::int64_t ms) { return Timestamp(ms * 1'000'000LL); }
}