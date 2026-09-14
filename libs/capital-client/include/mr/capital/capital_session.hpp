#pragma once
#include "mr/common/clock.hpp"
#include <string>

namespace mr {

struct CapitalSession {
    std::string cst;
    std::string security_token;
    std::string api_key;
    std::string identifier;
    std::string password;
    std::string account_id;
    bool active{false};
    int last_http_status{0};
    SteadyTimestamp cooldown_until{};
    std::uint64_t reconnect_count{0};
    std::uint64_t auth_failure_count{0};
    std::uint64_t rate_limit_count{0};
};

}  // namespace mr
