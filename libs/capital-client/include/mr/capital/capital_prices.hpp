#pragma once
#include "mr/capital/capital_quote.hpp"
#include <vector>
namespace mr {
struct CapitalPriceBar { Timestamp time{}; double open{0}, high{0}, low{0}, close{0}; };
using CapitalPriceHistory = std::vector<CapitalPriceBar>;
}