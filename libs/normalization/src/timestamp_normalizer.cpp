#include "mr/normalization/timestamp_normalizer.hpp"
namespace mr {
Timestamp TimestampNormalizer::normalize(const MarketEvent& e, Timestamp receive) {
    if (e.exchange_timestamp.count() > 0) return e.exchange_timestamp;
    if (e.provider_timestamp.count() > 0) return e.provider_timestamp;
    return receive;
}
}