#include "mr/data_quality/anomaly.hpp"
#include "mr/common/math.hpp"
namespace mr {
void AnomalyDetector::configure(double max_spread, double max_jump_pct) {
    max_spread_ = max_spread; max_jump_pct_ = max_jump_pct;
}
AnomalyResult AnomalyDetector::check(const NormalizedEvent& e, double prev_mid) {
    AnomalyResult r;
    if (e.bid && e.ask) {
        r.crossed = *e.bid >= *e.ask;
        r.wide_spread = (*e.ask - *e.bid) > max_spread_;
        double mid = (*e.bid + *e.ask) * 0.5;
        if (prev_mid > 0) r.price_jump = std::abs(pct_change(prev_mid, mid)) > max_jump_pct_;
    }
    return r;
}
}