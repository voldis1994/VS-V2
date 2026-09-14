#pragma once

#include "mr/common/id.hpp"

namespace mr {

struct CapitalQuote {
    double bid{0};
    double ask{0};
    Timestamp ts{};
    bool valid{false};
};

}  // namespace mr
