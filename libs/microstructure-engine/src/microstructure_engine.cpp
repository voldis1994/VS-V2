#include "mr/microstructure_engine/microstructure_engine.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

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

void MicrostructureEngine::reset() {
    f_ = {};
    bars_.clear();
    swings_.clear();
    last_open_time_.reset();
    breakout_armed_up_ = false;
    breakout_armed_down_ = false;
    trade_count_ = 0;
    quote_count_ = 0;
}

void MicrostructureEngine::update(const NormalizedEvent& e) {
    // RAW quote / trade path — book metrics only. Never confirms setup.
    if (e.bid && e.ask) {
        f_.spread = *e.ask - *e.bid;
        const double bs = e.bid_size.value_or(1.0);
        const double asz = e.ask_size.value_or(1.0);
        f_.microprice = (*e.bid * asz + *e.ask * bs) / (bs + asz);
        f_.bid_ask_imbalance = (bs - asz) / (bs + asz);
    }
    if (e.type == MarketEventType::Trade) {
        ++trade_count_;
        if (e.trade_size && e.last && e.bid && e.ask) {
            const double mid = (*e.bid + *e.ask) * 0.5;
            if (*e.last >= mid) f_.aggressive_buy_pressure += *e.trade_size;
            else f_.aggressive_sell_pressure += *e.trade_size;
        }
    } else {
        ++quote_count_;
    }
    // Explicitly do NOT set setup_confirmed / sequence evidence from quotes.
}

void MicrostructureEngine::rebuild_all_swings() {
    swings_.clear();
    if (bars_.size() < static_cast<std::size_t>(kPivotLeft + kPivotRight + 1)) return;
    const std::size_t last_pivot = bars_.size() - 1 - static_cast<std::size_t>(kPivotRight);
    for (std::size_t i = static_cast<std::size_t>(kPivotLeft); i <= last_pivot; ++i) {
        const double hi = bars_[i].high;
        const double lo = bars_[i].low;
        bool is_high = true, is_low = true;
        for (int d = 1; d <= kPivotLeft; ++d) {
            if (!(hi > bars_[i - static_cast<std::size_t>(d)].high)) is_high = false;
            if (!(lo < bars_[i - static_cast<std::size_t>(d)].low)) is_low = false;
        }
        for (int d = 1; d <= kPivotRight; ++d) {
            if (!(hi > bars_[i + static_cast<std::size_t>(d)].high)) is_high = false;
            if (!(lo < bars_[i + static_cast<std::size_t>(d)].low)) is_low = false;
        }
        auto last_of = [&](PivotKind kind) -> const SwingPoint* {
            for (auto it = swings_.rbegin(); it != swings_.rend(); ++it) {
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
                sp.label = (hi > prev->price) ? 1 : 3;
            }
            swings_.push_back(sp);
        }
        if (is_low) {
            SwingPoint sp;
            sp.kind = PivotKind::Low;
            sp.price = lo;
            sp.bar_index = i;
            if (const auto* prev = last_of(PivotKind::Low)) {
                sp.label = (lo > prev->price) ? 2 : 4;
            }
            swings_.push_back(sp);
        }
    }
}

void MicrostructureEngine::on_closed_10s(const Candle& closed) {
    // One-shot: each open_time processed exactly once.
    if (last_open_time_.has_value() && closed.open_time == *last_open_time_) {
        return;
    }

    bars_.push_back(closed);
    bool trimmed = false;
    while (bars_.size() > kMaxBars) {
        bars_.pop_front();
        trimmed = true;
    }

    last_open_time_ = closed.open_time;
    ++f_.closed_10s_count;
    f_.has_authority = true;
    f_.authority_tf = Timeframe::Second10;
    f_.setup_confirmed = true;  // CLOSED 10s only confirms evidence

    if (trimmed) rebuild_all_swings();
    else detect_new_swings();
    recompute_from_sequence();
}

void MicrostructureEngine::detect_new_swings() {
    if (bars_.size() < static_cast<std::size_t>(kPivotLeft + kPivotRight + 1)) return;
    const std::size_t i = bars_.size() - 1 - static_cast<std::size_t>(kPivotRight);
    if (i < static_cast<std::size_t>(kPivotLeft)) return;
    for (const auto& s : swings_) {
        if (s.bar_index == i) return;
    }

    const double hi = bars_[i].high;
    const double lo = bars_[i].low;
    bool is_high = true, is_low = true;
    for (int d = 1; d <= kPivotLeft; ++d) {
        if (!(hi > bars_[i - static_cast<std::size_t>(d)].high)) is_high = false;
        if (!(lo < bars_[i - static_cast<std::size_t>(d)].low)) is_low = false;
    }
    for (int d = 1; d <= kPivotRight; ++d) {
        if (!(hi > bars_[i + static_cast<std::size_t>(d)].high)) is_high = false;
        if (!(lo < bars_[i + static_cast<std::size_t>(d)].low)) is_low = false;
    }

    auto last_of = [&](PivotKind kind) -> const SwingPoint* {
        for (auto it = swings_.rbegin(); it != swings_.rend(); ++it) {
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
            sp.label = (hi > prev->price) ? 1 : 3;
        }
        swings_.push_back(sp);
    }
    if (is_low) {
        SwingPoint sp;
        sp.kind = PivotKind::Low;
        sp.price = lo;
        sp.bar_index = i;
        if (const auto* prev = last_of(PivotKind::Low)) {
            sp.label = (lo > prev->price) ? 2 : 4;
        }
        swings_.push_back(sp);
    }
}

void MicrostructureEngine::recompute_from_sequence() {
    if (bars_.empty()) return;
    const Candle& last = bars_.back();
    const double mid = std::max(std::abs(last.close), 1e-9);
    const double rng = std::max(last.range(), 1e-12);

    f_.body_pct = last.body_pct();
    const double upper = last.high - std::max(last.open, last.close);
    const double lower = std::min(last.open, last.close) - last.low;
    f_.upper_wick_pct = upper / mid;
    f_.lower_wick_pct = lower / mid;
    f_.candle_strength = clamp01(std::abs(last.body()) / rng);

    f_.momentum = 0;
    f_.acceleration = 0;
    f_.deceleration = 0;
    if (bars_.size() >= 2) {
        const double v1 = bars_[bars_.size() - 1].close - bars_[bars_.size() - 2].close;
        f_.momentum = v1 / mid;
        if (bars_.size() >= 3) {
            const double v0 = bars_[bars_.size() - 2].close - bars_[bars_.size() - 3].close;
            const double accel = (v1 - v0) / mid;
            f_.acceleration = std::max(0.0, accel);
            f_.deceleration = std::max(0.0, -accel);
        }
    }

    if (last.body() > 0) {
        f_.acceptance = clamp01(f_.candle_strength);
        f_.rejection = clamp01(std::min(1.0, f_.upper_wick_pct * 200.0));
    } else if (last.body() < 0) {
        f_.rejection = clamp01(f_.candle_strength);
        f_.acceptance = clamp01(std::min(1.0, f_.lower_wick_pct * 200.0));
    } else {
        f_.acceptance *= 0.5;
        f_.rejection *= 0.5;
    }

    f_.reclaim = 0;
    if (bars_.size() >= 2) {
        const Candle& prev = bars_[bars_.size() - 2];
        const double prev_mid = 0.5 * (prev.high + prev.low);
        if (prev.close < prev_mid && last.close > prev_mid && last.body() > 0) {
            f_.reclaim = clamp01(f_.candle_strength + 0.2);
        } else if (prev.close > prev_mid && last.close < prev_mid && last.body() < 0) {
            f_.reclaim = clamp01(f_.candle_strength + 0.2);
        }
    }
    f_.rejection_proxy = f_.rejection;
    f_.reclaim_proxy = f_.reclaim;

    f_.local_hh = f_.local_hl = f_.local_lh = f_.local_ll = 0;
    f_.last_swing_high = f_.last_swing_low = 0;
    f_.swing_count = static_cast<std::uint32_t>(swings_.size());
    int hh = 0, hl = 0, lh = 0, ll = 0;
    for (const auto& s : swings_) {
        if (s.kind == PivotKind::High) f_.last_swing_high = s.price;
        if (s.kind == PivotKind::Low) f_.last_swing_low = s.price;
        if (s.label == 1) { ++hh; f_.local_hh = 1; }
        if (s.label == 2) { ++hl; f_.local_hl = 1; }
        if (s.label == 3) { ++lh; f_.local_lh = 1; }
        if (s.label == 4) { ++ll; f_.local_ll = 1; }
    }
    const int bull = hh + hl;
    const int bear = lh + ll;
    if (bull >= 2 && bull > bear && hh >= 1 && hl >= 1) f_.swing_state = 1;
    else if (bear >= 2 && bear > bull && lh >= 1 && ll >= 1) f_.swing_state = -1;
    else f_.swing_state = 0;

    const std::size_t window = std::min<std::size_t>(bars_.size(), 12);
    const std::size_t start = bars_.size() - window;
    const std::size_t end_excl = bars_.size() > 1 ? bars_.size() - 1 : bars_.size();
    double hi = bars_[start].high;
    double lo = bars_[start].low;
    for (std::size_t i = start; i < end_excl; ++i) {
        hi = std::max(hi, bars_[i].high);
        lo = std::min(lo, bars_[i].low);
    }
    if (end_excl == start) {
        hi = last.high;
        lo = last.low;
    }
    const double width = std::max(0.0, hi - lo);
    f_.breakout_up = width > 0 && last.close > hi;
    f_.breakout_down = width > 0 && last.close < lo;
    f_.breakout_strength = 0;
    if (f_.breakout_up) {
        f_.breakout_strength = clamp01((last.close - hi) / std::max(width, 1e-9));
    } else if (f_.breakout_down) {
        f_.breakout_strength = clamp01((lo - last.close) / std::max(width, 1e-9));
    }

    f_.failed_breakout = 0;
    f_.failed_breakout_up = false;
    f_.failed_breakout_down = false;
    if (breakout_armed_up_ && last.close <= hi && last.body() < 0) {
        f_.failed_breakout_up = true;
        f_.failed_breakout = clamp01(0.4 + f_.candle_strength);
    }
    if (breakout_armed_down_ && last.close >= lo && last.body() > 0) {
        f_.failed_breakout_down = true;
        f_.failed_breakout = clamp01(0.4 + f_.candle_strength);
    }
    breakout_armed_up_ = f_.breakout_up;
    breakout_armed_down_ = f_.breakout_down;

    const double atr_s = mean_range(bars_, 3);
    const double atr_l = mean_range(bars_, 10);
    f_.volatility = atr_s / mid;
    const double long_safe = std::max(atr_l, mid * 1e-6);
    f_.compression = (atr_l > 0 && atr_s < long_safe * 0.65) ? clamp01(1.0 - atr_s / long_safe) : 0;
    f_.expansion = (atr_l > 0 && atr_s > long_safe * 1.35) ? clamp01(atr_s / long_safe - 1.0) : 0;

    f_.pullback_depth = 0;
    f_.pullback_quality = 0;
    if (f_.swing_state > 0 && f_.last_swing_high > f_.last_swing_low && f_.last_swing_low > 0) {
        if (last.close < f_.last_swing_high && last.close > f_.last_swing_low) {
            const double span = f_.last_swing_high - f_.last_swing_low;
            f_.pullback_depth = clamp01((f_.last_swing_high - last.close) / span);
            f_.pullback_quality = clamp01((1.0 - f_.pullback_depth) * f_.candle_strength
                                          + (last.body() < 0 ? 0.2 : 0.0));
        }
    } else if (f_.swing_state < 0 && f_.last_swing_high > f_.last_swing_low) {
        if (last.close > f_.last_swing_low && last.close < f_.last_swing_high) {
            const double span = f_.last_swing_high - f_.last_swing_low;
            f_.pullback_depth = clamp01((last.close - f_.last_swing_low) / span);
            f_.pullback_quality = clamp01((1.0 - f_.pullback_depth) * f_.candle_strength
                                          + (last.body() > 0 ? 0.2 : 0.0));
        }
    }

    double buy_sum = 0, sell_sum = 0;
    const std::size_t pstart = bars_.size() > 6 ? bars_.size() - 6 : 0;
    for (std::size_t i = pstart; i < bars_.size(); ++i) {
        const auto& b = bars_[i];
        const double br = std::max(b.range(), 1e-12);
        const double body = b.body();
        const double uw = b.high - std::max(b.open, b.close);
        const double lw = std::min(b.open, b.close) - b.low;
        double buy = 0.5, sell = 0.5;
        const double bf = clamp01(std::abs(body) / br);
        if (body >= 0) {
            buy = 0.5 + 0.5 * bf;
            sell = 1.0 - buy;
        } else {
            sell = 0.5 + 0.5 * bf;
            buy = 1.0 - sell;
        }
        const double wick_bal = (lw - uw) / br;
        buy = clamp01(buy + 0.1 * wick_bal);
        sell = clamp01(sell - 0.1 * wick_bal);
        buy_sum += buy;
        sell_sum += sell;
    }
    const double pn = static_cast<double>(bars_.size() - pstart);
    f_.buyer_pressure = buy_sum / pn;
    f_.seller_pressure = sell_sum / pn;
    f_.pressure_delta = f_.buyer_pressure - f_.seller_pressure;

    f_.continuation = clamp01(std::abs(f_.momentum) * 50.0 * f_.candle_strength
                              + (f_.acceleration > 0 ? 0.2 : 0.0));
    f_.exhaustion = clamp01(f_.deceleration * 40.0 + f_.rejection * 0.5
                            + (std::abs(f_.pressure_delta) > 0.35 ? 0.15 : 0.0));
    f_.exhaustion_proxy = f_.exhaustion;

    // Evidence blend — not a BUY/SELL threshold.
    f_.entry_timing_quality = clamp01(
        0.25 * f_.candle_strength
        + 0.20 * f_.continuation
        + 0.15 * (1.0 - f_.exhaustion)
        + 0.15 * clamp01(std::abs(f_.pressure_delta) * 2.0)
        + 0.10 * f_.acceptance
        + 0.10 * f_.reclaim
        + 0.05 * (1.0 - f_.failed_breakout));
}

}  // namespace mr
