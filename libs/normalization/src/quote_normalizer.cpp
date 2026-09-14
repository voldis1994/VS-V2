#include "mr/normalization/quote_normalizer.hpp"
namespace mr {
NormalizedEvent QuoteNormalizer::normalize_quote(const MarketEvent& e) {
    auto out = base_.normalize(e);
    if (!out.bid || !out.ask) out.quality |= static_cast<DataQualityFlags>(DataQualityFlag::MissingField);
    return out;
}
}