#pragma once
#include "mr/market_types/candle.hpp"
#include <string>
namespace mr {
struct ValidationResult { bool ok{true}; std::string reason; };
class CandleValidator {
public:
    ValidationResult validate(const Candle& c) const;
};
}