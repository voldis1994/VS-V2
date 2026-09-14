#pragma once
#include <cstdint>
namespace mr {
struct RetryPolicy { std::uint32_t max_attempts{3}; std::uint64_t backoff_ms{100}; };
}
