#pragma once
#include "mr/data_quality/quality_state.hpp"
#include <unordered_map>
namespace mr {
class FreshnessTracker {
public:
    void observe(SourceId src, Timestamp ts, double stale_ms);
    [[nodiscard]] bool is_stale(SourceId src) const;
    [[nodiscard]] double age_ms(SourceId src, Timestamp now) const;
private:
    std::unordered_map<SourceId, Timestamp> last_;
    std::unordered_map<SourceId, double> threshold_;
};
}