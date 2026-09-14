#include "mr/normalization/candle_normalizer.hpp"
namespace mr {
NormalizedCandle CandleNormalizer::normalize(const Candle& c, Timestamp ts) {
    NormalizedCandle n = static_cast<NormalizedCandle>(c);
    n.normalized_time = ts;
    return n;
}
}