#include "mr/normalization/normalizer.hpp"
namespace mr {
NormalizedEvent Normalizer::normalize(const MarketEvent& event) {
    NormalizedEvent out = static_cast<NormalizedEvent>(event);
    out.processing_start = clock_.utc_now();
    if (event.exchange_timestamp.count() > 0) {
        out.normalized_timestamp = event.exchange_timestamp;
    } else if (event.provider_timestamp.count() > 0) {
        out.normalized_timestamp = event.provider_timestamp;
    } else {
        out.normalized_timestamp = out.processing_start;
    }
    if (event.source < 256) {
        auto& last = last_seq_[event.source];
        if (event.sequence > 0 && last > 0) {
            if (event.sequence == last) out.quality |= static_cast<DataQualityFlags>(DataQualityFlag::Duplicate);
            else if (event.sequence < last) out.quality |= static_cast<DataQualityFlags>(DataQualityFlag::OutOfOrder);
            else if (event.sequence > last + 1) out.quality |= static_cast<DataQualityFlags>(DataQualityFlag::SequenceGap);
        }
        if (event.sequence > 0) last = event.sequence;
    }
    if (out.bid && out.ask && *out.bid >= *out.ask) {
        out.quality |= static_cast<DataQualityFlags>(DataQualityFlag::Crossed);
    }
    out.processing_end = clock_.utc_now();
    return out;
}
}