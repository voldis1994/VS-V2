#pragma once
#include "mr/common/id.hpp"
namespace mr {
struct CapitalQuote { double bid{0}, ask{0}, Timestamp timestamp{}; bool valid{false}; };
}