#pragma once
#include "mr/common/id.hpp"
namespace mr {
struct ConsensusQuote {
    double mid{0}, spread{0}, confidence{0};
    std::uint32_t sources{0};
    [[nodiscard]] bool valid() const { return mid > 0 && sources > 0; }
};
}