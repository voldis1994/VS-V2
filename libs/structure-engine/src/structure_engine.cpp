#include "mr/structure_engine/structure_engine.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace mr {
namespace {

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

double mean_range(const std::deque<Candle>& bars, std::size_t n) {
    if (bars.empty() || n == 0) return 0;
    const std::size_t start = bars.size() > n ? bars.size() - n : 0;
    double sum = 0;
    std::size_t count = 0;
    for (std::size_t i = start; i < bars.size(); ++i) {
        sum += std::max(0.0, bars[i].range());
        ++count;
    }
    return count ? sum / static_cast<double>(count) : 0;
}

}  // namespace

int StructureEngine::lane_index(Timeframe tf) {
    switch (tf) {
        case Timeframe::Minute1: return 0;
        case Timeframe::Minute5: return 1;
        case Timeframe::Minute15: return 2;
        case Timeframe::Hour1: return 3;
        default: return -1;
    }
}

bool StructureEngine::is_authority_tf(Timeframe tf) {
    return static_cast<std::uint32_t>(tf) >= static_cast<std::uint32_t>(Timeframe::Minute1)
        && lane_index(tf) >= 0;
}

void StructureEngine::reset() {
    lanes_ = {};
    composite_ = {};
    has_authority_ = false;
    authority_tf_ = Timeframe::Minute1;
    authority_updates_ = 0;
}

StructureFeatures StructureEngine::snapshot(Timeframe tf) const {
    const int idx = lane_index(tf);
    if (idx < 0) return {};
    return lanes_[static_cast<std::size_t>(idx)].features;
}

void StructureEngine::on_authority_close(const Candle& closed, Timeframe tf,
                                         const PriceDynamics& /*pd*/) {
    // Refuse sub-minute / non-authority TFs — never rewrite broad structure.
    if (!is_authority_tf(tf)) return;

    const int idx = lane_index(tf);
    auto& lane = lanes_[static_cast<std::size_t>(idx)];
    lane.bars.push_back(closed);
    bool trimmed = false;
    while (lane.bars.size() > kMaxBars) {
        lane.bars.pop_front();
        trimmed = true;
    }

    lane.has_data = true;
    has_authority_ = true;
    authority_tf_ = tf;
    ++authority_updates_;

    if (trimmed) {
        rebuild_all_swings(lane);
    } else {
        detect_new_swings(lane);
    }
    recompute_lane(lane, tf);
    rebuild_composite();
}

void StructureEngine::rebuild_all_swings(TfLane& lane) {
    lane.swings.clear();
    if (lane.bars.size() < static_cast<std::size_t>(kPivotLeft + kPivotRight + 1)) return;
    const std::size_t last_pivot =
        lane.bars.size() - 1 - static_cast<std::size_t>(kPivotRight);
    for (std::size_t fake = static_cast<std::size_t>(kPivotLeft) + static_cast<std::size_t>(kPivotRight);
         fake < lane.bars.size(); ++fake) {
        // Temporarily treat prefix size = fake+1 so detect_new_swings sees pivot at fake-right.
        // Inline pivot scan instead:
        const std::size_t i = fake - static_cast<std::size_t>(kPivotRight);
        if (i < static_cast<std::size_t>(kPivotLeft) || i > last_pivot) continue;

        const double hi = lane.bars[i].high;
        const double lo = lane.bars[i].low;
        bool is_high = true;
        bool is_low = true;
        for (int d = 1; d <= kPivotLeft; ++d) {
            if (!(hi > lane.bars[i - static_cast<std::size_t>(d)].high)) is_high = false;
            if (!(lo < lane.bars[i - static_cast<std::size_t>(d)].low)) is_low = false;
        }
        for (int d = 1; d <= kPivotRight; ++d) {
            if (i + static_cast<std::size_t>(d) >= lane.bars.size()) {
                is_high = false;
                is_low = false;
                break;
            }
            if (!(hi > lane.bars[i + static_cast<std::size_t>(d)].high)) is_high = false;
            if (!(lo < lane.bars[i + static_cast<std::size_t>(d)].low)) is_low = false;
        }

        auto last_of = [&](PivotKind kind) -> const SwingPoint* {
            for (auto it = lane.swings.rbegin(); it != lane.swings.rend(); ++it) {
                if (it->kind == kind) return &(*it);
            }
            return nullptr;
        };

        if (is_high) {
            SwingPoint sp;
            sp.kind = PivotKind::High;
            sp.price = hi;
            sp.bar_index = i;
            if (const auto* prev = last_of(PivotKind::High)) {
                sp.label = (hi > prev->price) ? SwingLabel::HH : SwingLabel::LH;
            }
            lane.swings.push_back(sp);
        }
        if (is_low) {
            SwingPoint sp;
            sp.kind = PivotKind::Low;
            sp.price = lo;
            sp.bar_index = i;
            if (const auto* prev = last_of(PivotKind::Low)) {
                sp.label = (lo > prev->price) ? SwingLabel::HL : SwingLabel::LL;
            }
            lane.swings.push_back(sp);
        }
    }
}

void StructureEngine::detect_new_swings(TfLane& lane) {
    const auto& bars = lane.bars;
    if (bars.size() < static_cast<std::size_t>(kPivotLeft + kPivotRight + 1)) return;

    const std::size_t i = bars.size() - 1 - static_cast<std::size_t>(kPivotRight);
    if (i < static_cast<std::size_t>(kPivotLeft)) return;

    // Skip if this bar index was already recorded as a swing.
    for (const auto& s : lane.swings) {
        if (s.bar_index == i) return;
    }

    const double hi = bars[i].high;
    const double lo = bars[i].low;
    bool is_high = true;
    bool is_low = true;
    for (int d = 1; d <= kPivotLeft; ++d) {
        if (!(hi > bars[i - static_cast<std::size_t>(d)].high)) is_high = false;
        if (!(lo < bars[i - static_cast<std::size_t>(d)].low)) is_low = false;
    }
    for (int d = 1; d <= kPivotRight; ++d) {
        if (!(hi > bars[i + static_cast<std::size_t>(d)].high)) is_high = false;
        if (!(lo < bars[i + static_cast<std::size_t>(d)].low)) is_low = false;
    }

    auto last_of = [&](PivotKind kind) -> const SwingPoint* {
        for (auto it = lane.swings.rbegin(); it != lane.swings.rend(); ++it) {
            if (it->kind == kind) return &(*it);
        }
        return nullptr;
    };

    if (is_high) {
        SwingPoint sp;
        sp.kind = PivotKind::High;
        sp.price = hi;
        sp.bar_index = i;
        if (const auto* prev = last_of(PivotKind::High)) {
            sp.label = (hi > prev->price) ? SwingLabel::HH : SwingLabel::LH;
        } else {
            sp.label = SwingLabel::None;
        }
        lane.swings.push_back(sp);
    }
    if (is_low) {
        SwingPoint sp;
        sp.kind = PivotKind::Low;
        sp.price = lo;
        sp.bar_index = i;
        if (const auto* prev = last_of(PivotKind::Low)) {
            sp.label = (lo > prev->price) ? SwingLabel::HL : SwingLabel::LL;
        } else {
            sp.label = SwingLabel::None;
        }
        lane.swings.push_back(sp);
    }
}

void StructureEngine::classify_trend(TfLane& lane) {
    auto& f = lane.features;
    f.swing_count = static_cast<std::uint32_t>(lane.swings.size());
    f.last_swing = SwingLabel::None;
    f.last_swing_high = 0;
    f.last_swing_low = 0;

    int hh = 0, hl = 0, lh = 0, ll = 0;
    for (const auto& s : lane.swings) {
        if (s.kind == PivotKind::High) f.last_swing_high = s.price;
        if (s.kind == PivotKind::Low) f.last_swing_low = s.price;
        f.last_swing = s.label;
        switch (s.label) {
            case SwingLabel::HH: ++hh; break;
            case SwingLabel::HL: ++hl; break;
            case SwingLabel::LH: ++lh; break;
            case SwingLabel::LL: ++ll; break;
            default: break;
        }
    }

    // Need a multi-swing sequence — not a single candle.
    const int bull = hh + hl;
    const int bear = lh + ll;
    TrendBias bias = TrendBias::Unknown;
    if (bull >= 2 && bull > bear && hh >= 1 && hl >= 1) {
        bias = TrendBias::Up;
        f.swing_state = 1;
    } else if (bear >= 2 && bear > bull && lh >= 1 && ll >= 1) {
        bias = TrendBias::Down;
        f.swing_state = -1;
    } else if (lane.bars.size() >= 5) {
        bias = TrendBias::Range;
        f.swing_state = 0;
    } else {
        bias = TrendBias::Unknown;
        f.swing_state = 0;
    }

    if (bias == lane.prev_trend && bias != TrendBias::Unknown) {
        ++lane.bars_in_trend;
    } else {
        lane.bars_in_trend = (bias == TrendBias::Unknown) ? 0 : 1;
        lane.prev_trend = bias;
    }

    f.trend_direction = bias;
    f.trend_age = lane.bars_in_trend;

    const double denom = static_cast<double>(std::max(1, bull + bear));
    const double dominance = std::abs(static_cast<double>(bull - bear)) / denom;
    f.trend_strength = clamp01(dominance * (bias == TrendBias::Range ? 0.25 : 1.0)
                               * clamp01(static_cast<double>(f.swing_count) / 4.0));

    // Structural invalidation: break of last protective swing.
    f.structural_invalidation = 0;
    if (!lane.bars.empty()) {
        const double px = lane.bars.back().close;
        if (bias == TrendBias::Up && f.last_swing_low > 0 && px < f.last_swing_low) {
            f.structural_invalidation = clamp01((f.last_swing_low - px)
                / std::max(f.last_swing_high - f.last_swing_low, 1e-9));
        } else if (bias == TrendBias::Down && f.last_swing_high > 0 && px > f.last_swing_high) {
            f.structural_invalidation = clamp01((px - f.last_swing_high)
                / std::max(f.last_swing_high - f.last_swing_low, 1e-9));
        }
    }

    // Structure quality: swing clarity + age + low invalidation.
    f.structure_quality = clamp01(
        0.35 * clamp01(static_cast<double>(f.swing_count) / 6.0)
        + 0.35 * f.trend_strength
        + 0.20 * clamp01(static_cast<double>(f.trend_age) / 8.0)
        + 0.10 * (1.0 - f.structural_invalidation));
}

void StructureEngine::classify_range_vol_breakout(TfLane& lane) {
    auto& f = lane.features;
    auto& bars = lane.bars;
    if (bars.empty()) return;

    const Candle& last = bars.back();
    // Structure window excludes the latest bar for breakout reference.
    const std::size_t window = std::min<std::size_t>(bars.size(), 20);
    const std::size_t start = bars.size() - window;
    const std::size_t end_excl = bars.size() > 1 ? bars.size() - 1 : bars.size();

    double hi = bars[start].high;
    double lo = bars[start].low;
    for (std::size_t i = start; i < end_excl; ++i) {
        hi = std::max(hi, bars[i].high);
        lo = std::min(lo, bars[i].low);
    }
    if (end_excl == start) {
        hi = last.high;
        lo = last.low;
    }

    f.structure_high = hi;
    f.structure_low = lo;
    f.range_width = std::max(0.0, hi - lo);
    const double mid = std::max(std::abs(last.close), 1e-9);
    f.range_boundary_distance = (f.range_width > 0)
        ? std::min(last.close - lo, hi - last.close)
        : 0;
    f.range_position = (f.range_width > 0)
        ? clamp01((last.close - lo) / f.range_width)
        : 0.5;
    f.in_range = (last.close <= hi && last.close >= lo);

    // Volatility from OHLC ranges (ATR-like) — NOT acceleration (old VS bug).
    const double atr_short = mean_range(bars, 4);
    const double atr_long = mean_range(bars, 12);
    f.volatility = atr_short / mid;
    const double long_safe = std::max(atr_long, mid * 1e-6);
    f.compression = (atr_long > 0 && atr_short < long_safe * 0.65)
        ? clamp01(1.0 - atr_short / long_safe)
        : 0;
    f.expansion = (atr_long > 0 && atr_short > long_safe * 1.35)
        ? clamp01(atr_short / long_safe - 1.0)
        : 0;

    const bool breakout_up = last.close > hi && f.range_width > 0;
    const bool breakout_down = last.close < lo && f.range_width > 0;
    f.breakout_up = breakout_up;
    f.breakout_down = breakout_down;
    f.breakout_active = breakout_up || breakout_down;
    f.breakout_strength = 0;
    if (breakout_up) {
        f.breakout_strength = clamp01((last.close - hi) / std::max(f.range_width, 1e-9)
                                      + f.expansion);
    } else if (breakout_down) {
        f.breakout_strength = clamp01((lo - last.close) / std::max(f.range_width, 1e-9)
                                      + f.expansion);
    }

    // Failed breakout: prior bar closed outside, current closes back inside.
    f.failed_breakout = 0;
    f.failed_breakout_up = false;
    f.failed_breakout_down = false;
    if (bars.size() >= 3 && f.range_width > 0) {
        // Recompute prior range excluding last two bars for the prior close test.
        const std::size_t pend = bars.size() - 2;
        const std::size_t pstart = bars.size() > 21 ? bars.size() - 21 : 0;
        double phi = bars[pstart].high;
        double plo = bars[pstart].low;
        for (std::size_t i = pstart; i < pend; ++i) {
            phi = std::max(phi, bars[i].high);
            plo = std::min(plo, bars[i].low);
        }
        const Candle& prior = bars[bars.size() - 2];
        const bool prior_up = prior.close > phi;
        const bool prior_down = prior.close < plo;
        if (prior_up && f.in_range && last.close < prior.close) {
            f.failed_breakout_up = true;
            f.failed_breakout = clamp01((phi - last.close) / std::max(phi - plo, 1e-9) + 0.35);
        } else if (prior_down && f.in_range && last.close > prior.close) {
            f.failed_breakout_down = true;
            f.failed_breakout = clamp01((last.close - plo) / std::max(phi - plo, 1e-9) + 0.35);
        }
    }

    if (lane.breakout_armed_up && f.in_range && last.body() < 0) {
        f.failed_breakout_up = true;
        f.failed_breakout = std::max(f.failed_breakout, 0.55);
    }
    if (lane.breakout_armed_down && f.in_range && last.body() > 0) {
        f.failed_breakout_down = true;
        f.failed_breakout = std::max(f.failed_breakout, 0.55);
    }
    lane.breakout_armed_up = breakout_up;
    lane.breakout_armed_down = breakout_down;

    const double bp = last.body_pct();
    if (bp > 0.0002) {
        f.continuation_pressure = clamp01(std::abs(bp) * 200.0);
        f.acceptance = f.continuation_pressure;
        f.rejection = 0;
    } else if (bp < -0.0002) {
        f.rejection = clamp01(std::abs(bp) * 200.0);
        f.continuation_pressure = -f.rejection;
        f.acceptance = 0;
    } else {
        f.continuation_pressure *= 0.5;
    }
}

void StructureEngine::classify_pullback_reversal(TfLane& lane) {
    auto& f = lane.features;
    if (lane.bars.empty()) return;
    const Candle& last = lane.bars.back();

    f.in_pullback = false;
    f.pullback_depth = 0;

    // Pullback from multi-swing trend — opposing bar inside protective swing.
    if (f.trend_direction == TrendBias::Up && f.last_swing_high > f.last_swing_low
        && f.last_swing_low > 0) {
        const bool retreated = last.close < f.last_swing_high
            && last.close >= f.last_swing_low
            && (last.body() < 0 || last.close < lane.bars[lane.bars.size() - 1].open);
        // Use recent high vs close.
        if (last.close < f.last_swing_high && last.close > f.last_swing_low) {
            const double span = f.last_swing_high - f.last_swing_low;
            f.pullback_depth = clamp01((f.last_swing_high - last.close) / span);
            f.in_pullback = f.pullback_depth > 0.15 && f.structural_invalidation < 0.5;
        }
        (void)retreated;
    } else if (f.trend_direction == TrendBias::Down && f.last_swing_high > f.last_swing_low
               && f.last_swing_high > 0) {
        if (last.close > f.last_swing_low && last.close < f.last_swing_high) {
            const double span = f.last_swing_high - f.last_swing_low;
            f.pullback_depth = clamp01((last.close - f.last_swing_low) / span);
            f.in_pullback = f.pullback_depth > 0.15 && f.structural_invalidation < 0.5;
        }
    }

    // Reversal evidence: opposing swing labels after established trend, or invalidation.
    f.reversal_candidate = 0;
    if (f.structural_invalidation > 0.35) {
        f.reversal_candidate = clamp01(0.4 + f.structural_invalidation);
    }
    if (f.trend_direction == TrendBias::Up && f.last_swing == SwingLabel::LL) {
        f.reversal_candidate = std::max(f.reversal_candidate, 0.55);
    }
    if (f.trend_direction == TrendBias::Down && f.last_swing == SwingLabel::HH) {
        f.reversal_candidate = std::max(f.reversal_candidate, 0.55);
    }
    if (f.failed_breakout > 0.5 && f.trend_strength > 0.3) {
        f.reversal_candidate = std::max(f.reversal_candidate, 0.45);
    }
}

void StructureEngine::recompute_lane(TfLane& lane, Timeframe tf) {
    lane.features = {};
    lane.features.authority_tf = tf;
    lane.features.bar_count = static_cast<std::uint32_t>(lane.bars.size());
    classify_trend(lane);
    classify_range_vol_breakout(lane);
    classify_pullback_reversal(lane);
}

void StructureEngine::rebuild_composite() {
    // Primary = finest authority lane with data (prefer 1m).
    const TfLane* primary = nullptr;
    for (std::size_t i = 0; i < lanes_.size(); ++i) {
        if (lanes_[i].has_data) {
            primary = &lanes_[i];
            break;
        }
    }
    if (!primary) {
        composite_ = {};
        return;
    }

    composite_ = primary->features;

    // HTF overlay: highest TF with data dominates trend context.
    const TfLane* htf = nullptr;
    for (std::size_t i = lanes_.size(); i-- > 0;) {
        if (lanes_[i].has_data && lanes_[i].features.bar_count >= 3) {
            htf = &lanes_[i];
            break;
        }
    }
    if (!htf || htf == primary) return;

    const auto& hf = htf->features;
    // Hierarchy: HTF trend bias overlays LTF pullback context.
    if (hf.trend_direction == TrendBias::Up || hf.trend_direction == TrendBias::Down) {
        composite_.trend_direction = hf.trend_direction;
        composite_.swing_state = hf.swing_state;
        composite_.trend_strength = std::max(composite_.trend_strength, hf.trend_strength * 0.85);
        // LTF pullback against HTF trend is still a pullback in HTF direction.
        if (primary->features.in_pullback
            || (hf.trend_direction == TrendBias::Up && primary->features.swing_state < 0)
            || (hf.trend_direction == TrendBias::Down && primary->features.swing_state > 0)) {
            composite_.in_pullback = true;
            composite_.pullback_depth = std::max(composite_.pullback_depth,
                                                 primary->features.pullback_depth);
        }
        if (hf.structural_invalidation > composite_.structural_invalidation) {
            composite_.structural_invalidation = hf.structural_invalidation;
        }
        composite_.structure_quality = clamp01(
            0.5 * composite_.structure_quality + 0.5 * hf.structure_quality);
    }
}

}  // namespace mr
