#pragma once
#include "mr/normalization/normalizer.hpp"
namespace mr {
class QuoteNormalizer {
public:
    explicit QuoteNormalizer(Normalizer& n) : base_(n) {}
    NormalizedEvent normalize_quote(const MarketEvent& e);
private:
    Normalizer& base_;
};
}