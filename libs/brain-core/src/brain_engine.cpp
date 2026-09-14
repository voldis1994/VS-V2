#include "mr/brain/brain_engine.hpp"
#include <algorithm>
#include <cmath>

namespace mr {

static double clamp(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}

BrainSnapshot BrainEngine::update(InstrumentId instrument, const std::vector<CandleBar>& bars, Timestamp ts) {
    BrainSnapshot snap;
    snap.instrument = instrument;
    snap.timestamp = ts;
    snap.bar_count = bars.size();
    if (bars.empty()) {
        latest_[instrument] = snap;
        return snap;
    }

    double first = bars.front().close;
    double last = bars.back().close;
    double roc = first != 0 ? (last - first) / first : 0.0;
    snap.scores.momentum = clamp(roc * 10.0, -1.0, 1.0);

    double swing_high = bars[0].high;
    double swing_low = bars[0].low;
    for (const auto& b : bars) {
        swing_high = std::max(swing_high, b.high);
        swing_low = std::min(swing_low, b.low);
    }
    snap.scores.structure = last > first ? 0.6 : (last < first ? -0.6 : 0.0);

    double range = swing_high - swing_low;
    snap.scores.pressure = range > 0 ? clamp((last - swing_low) / range * 2.0 - 1.0, -1.0, 1.0) : 0.0;

    double vol = 0;
    for (const auto& b : bars) vol += std::abs(b.close - b.open);
    snap.scores.behavior = clamp(vol / (bars.size() * (range > 0 ? range : 1.0)), 0.0, 1.0);

    snap.scores.impact = clamp(
        0.35 * snap.scores.structure + 0.35 * snap.scores.momentum + 0.2 * snap.scores.pressure + 0.1 * snap.scores.behavior,
        -1.0, 1.0);

    snap.scores.composite = snap.scores.impact;
    if (snap.scores.composite > 0.25) snap.scores.bias = "BULLISH";
    else if (snap.scores.composite < -0.25) snap.scores.bias = "BEARISH";
    else snap.scores.bias = "NEUTRAL";

    latest_[instrument] = snap;
    return snap;
}

}  // namespace mr
