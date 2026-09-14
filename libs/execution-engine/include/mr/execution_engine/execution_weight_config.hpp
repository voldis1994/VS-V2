#pragma once
#include <cstdint>

namespace mr {

/**
 * Execution lifecycle policy — Stage-8 calibratable.
 * Never a trading decision. Controls submit/retry/dedup only.
 */
struct ExecutionWeightConfig {
    std::uint32_t max_attempts{3};
    std::uint64_t backoff_ms{0};       // 0 = no sleep (PAPER/REPLAY/tests); LIVE sets >0
    std::uint64_t dedup_window_ms{2000};
    bool require_positive_quantity{true};

    static ExecutionWeightConfig defaults();
};

}  // namespace mr
