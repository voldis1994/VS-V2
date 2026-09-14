#include "mr/microstructure_engine/microstructure_engine.hpp"
#include <algorithm>
#include <cmath>

namespace mr {
namespace {

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

/** Soft map of non-negative raw into [0,1] via scale — no cliff threshold. */
double soft01(double raw, double scale) {
    const double x = std::max(0.0, raw);
    if (scale <= 1e-15) return clamp01(x);
    return clamp01(1.0 - std::exp(-x / scale));
}

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

MicrostructureEngine::MicrostructureEngine(MicroNormConfig cfg) : cfg_(std::move(cfg)) {}

void MicrostructureEngine::set_norm_config(MicroNormConfig cfg) {
    cfg_ = std::move(cfg);
    if (!bars_.empty()) recompute_from_sequence();
}

void MicrostructureEngine::reset() {
    f_ = {};
    bars_.clear();
    swings_.clear();
    last_open_time_.reset();
    prev_breakout_up_ = 0;
    prev_breakout_down_ = 0;
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
    f_.setup_confirmed = true;

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
    const double body = last.body();
    const double body_abs = std::abs(body);
    const double upper = last.high - std::max(last.open, last.close);
    const double lower = std::min(last.open, last.close) - last.low;
    const double upper_frac = clamp01(upper / rng);
    const double lower_frac = clamp01(lower / rng);
    const double body_frac = clamp01(body_abs / rng);
    const double bullish_mass = body > 0 ? body_frac : 0.0;
    const double bearish_mass = body < 0 ? body_frac : 0.0;

    // Geometry — continuous fractions of range / mid.
    f_.body_pct = last.body_pct();
    f_.upper_wick_pct = upper / mid;
    f_.lower_wick_pct = lower / mid;
    f_.candle_strength = body_frac;

    // Momentum / accel — continuous soft-normalized magnitudes.
    f_.momentum = 0;
    f_.acceleration = 0;
    f_.deceleration = 0;
    if (bars_.size() >= 2) {
        const double v1 = bars_[bars_.size() - 1].close - bars_[bars_.size() - 2].close;
        f_.momentum = v1 / mid;
        if (bars_.size() >= 3) {
            const double v0 = bars_[bars_.size() - 2].close - bars_[bars_.size() - 3].close;
            const double raw_accel = (v1 - v0) / mid;
            f_.acceleration = soft01(raw_accel, cfg_.accel_scale);
            f_.deceleration = soft01(-raw_accel, cfg_.accel_scale);
        }
    }
    const double mom_mag = soft01(std::abs(f_.momentum), cfg_.momentum_scale);

    // Acceptance / rejection from body vs opposing wick (continuous).
    f_.acceptance = clamp01(bullish_mass * (1.0 - upper_frac) + bearish_mass * (1.0 - lower_frac)
                            + (1.0 - body_frac) * 0.5 * (1.0 - std::abs(upper_frac - lower_frac)));
    f_.rejection = clamp01(bullish_mass * upper_frac + bearish_mass * lower_frac
                           + (1.0 - body_frac) * 0.5 * (upper_frac + lower_frac));

    // Reclaim — continuous mid-cross intensity (no discrete if-trigger).
    f_.reclaim = 0;
    if (bars_.size() >= 2) {
        const Candle& prev = bars_[bars_.size() - 2];
        const double prev_rng = std::max(prev.range(), 1e-12);
        const double prev_mid = 0.5 * (prev.high + prev.low);
        const double prev_signed = (prev.close - prev_mid) / prev_rng;
        const double curr_signed = (last.close - prev_mid) / prev_rng;
        const double flip = std::max(0.0, -prev_signed * curr_signed);
        f_.reclaim = clamp01(flip * body_frac * cfg_.reclaim_scale);
    }
    f_.rejection_proxy = f_.rejection;
    f_.reclaim_proxy = f_.reclaim;

    // Local swings — continuous label intensities (no bull>=2 gate).
    f_.last_swing_high = 0;
    f_.last_swing_low = 0;
    f_.swing_count = static_cast<std::uint32_t>(swings_.size());
    double hh = 0, hl = 0, lh = 0, ll = 0;
    for (const auto& s : swings_) {
        if (s.kind == PivotKind::High) f_.last_swing_high = s.price;
        if (s.kind == PivotKind::Low) f_.last_swing_low = s.price;
        if (s.label == 1) ++hh;
        if (s.label == 2) ++hl;
        if (s.label == 3) ++lh;
        if (s.label == 4) ++ll;
    }
    const double labeled = std::max(1.0, hh + hl + lh + ll);
    f_.local_hh = hh / labeled;
    f_.local_hl = hl / labeled;
    f_.local_lh = lh / labeled;
    f_.local_ll = ll / labeled;
    const double bull = hh + hl;
    const double bear = lh + ll;
    f_.swing_state = (bull + bear) > 0 ? (bull - bear) / (bull + bear) : 0.0;

    // Micro breakout — continuous penetration of prior window.
    const std::size_t window =
        std::min(bars_.size(), std::max<std::size_t>(1, cfg_.lookback_breakout));
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
    const double width = std::max(hi - lo, 1e-12);
    const double pen_up = std::max(0.0, (last.close - hi) / width);
    const double pen_down = std::max(0.0, (lo - last.close) / width);
    const double bo_up = clamp01(pen_up * cfg_.breakout_scale);
    const double bo_down = clamp01(pen_down * cfg_.breakout_scale);
    f_.breakout_strength = std::max(bo_up, bo_down);
    // Directional flags = geometric sign of penetration (not market-behavior gates).
    f_.breakout_up = pen_up > 0.0;
    f_.breakout_down = pen_down > 0.0;

    const double reverse_up = std::max(0.0, (hi - last.close) / width) * bearish_mass;
    const double reverse_down = std::max(0.0, (last.close - lo) / width) * bullish_mass;
    const double fail_up =
        clamp01(prev_breakout_up_ * reverse_up * cfg_.failed_breakout_scale);
    const double fail_down =
        clamp01(prev_breakout_down_ * reverse_down * cfg_.failed_breakout_scale);
    f_.failed_breakout = std::max(fail_up, fail_down);
    f_.failed_breakout_up = fail_up > 0.0;
    f_.failed_breakout_down = fail_down > 0.0;
    prev_breakout_up_ = bo_up;
    prev_breakout_down_ = bo_down;

    // Compression / expansion from ATR ratio — continuous, no 0.65/1.35 cliffs.
    const double atr_s = mean_range(bars_, cfg_.lookback_vol_short);
    const double atr_l = mean_range(bars_, cfg_.lookback_vol_long);
    f_.volatility = atr_s / mid;
    const double long_safe = std::max(atr_l, mid * 1e-6);
    const double vol_ratio = atr_s / long_safe;
    f_.compression = clamp01(std::max(0.0, 1.0 - vol_ratio) * cfg_.compression_scale);
    f_.expansion = clamp01(std::max(0.0, vol_ratio - 1.0) * cfg_.expansion_scale);

    // Pullback depth/quality — continuous in swing span.
    f_.pullback_depth = 0;
    f_.pullback_quality = 0;
    if (f_.last_swing_high > f_.last_swing_low && f_.last_swing_low > 0) {
        const double span = f_.last_swing_high - f_.last_swing_low;
        const double pos = clamp01((last.close - f_.last_swing_low) / span);
        const double up_bias = clamp01(0.5 + 0.5 * f_.swing_state);
        const double down_bias = clamp01(0.5 - 0.5 * f_.swing_state);
        f_.pullback_depth = clamp01((1.0 - pos) * up_bias + pos * down_bias);
        const double constructive =
            up_bias * bullish_mass + down_bias * bearish_mass + (1.0 - body_frac) * 0.25;
        f_.pullback_quality = clamp01((1.0 - f_.pullback_depth) * constructive);
    }

    // Buyer / seller pressure from CLOSED 10s bodies/wicks.
    double buy_sum = 0, sell_sum = 0;
    const std::size_t pwin = std::max<std::size_t>(1, cfg_.lookback_pressure);
    const std::size_t pstart = bars_.size() > pwin ? bars_.size() - pwin : 0;
    const double wp = std::clamp(cfg_.wick_pressure_weight, 0.0, 0.5);
    for (std::size_t i = pstart; i < bars_.size(); ++i) {
        const auto& b = bars_[i];
        const double br = std::max(b.range(), 1e-12);
        const double bf = clamp01(std::abs(b.body()) / br);
        const double uw = (b.high - std::max(b.open, b.close)) / br;
        const double lw = (std::min(b.open, b.close) - b.low) / br;
        double buy = 0.5 + 0.5 * (b.body() >= 0 ? bf : -bf);
        double sell = 1.0 - buy;
        const double wick_bal = lw - uw;
        buy = clamp01(buy + wp * wick_bal);
        sell = clamp01(sell - wp * wick_bal);
        buy_sum += buy;
        sell_sum += sell;
    }
    const double pn = static_cast<double>(bars_.size() - pstart);
    f_.buyer_pressure = buy_sum / pn;
    f_.seller_pressure = sell_sum / pn;
    f_.pressure_delta = f_.buyer_pressure - f_.seller_pressure;

    // Continuation vs exhaustion — continuous blends, no pressure cliff.
    const double cont_raw =
        mom_mag * f_.candle_strength + f_.acceleration * (1.0 - f_.deceleration);
    f_.continuation = soft01(cont_raw * cfg_.continuation_scale, 1.0);
    const double exh_raw = f_.deceleration + f_.rejection * 0.5
                           + soft01(std::abs(f_.pressure_delta), 1.0) * 0.25;
    f_.exhaustion = soft01(exh_raw * cfg_.exhaustion_scale, 1.0);
    f_.exhaustion_proxy = f_.exhaustion;

    // Entry timing quality — configurable evidence blend (not a trade trigger).
    const auto& w = cfg_.entry_timing_weights;
    double wsum = 0;
    for (double wi : w) wsum += std::max(0.0, wi);
    if (wsum <= 1e-15) wsum = 1.0;
    f_.entry_timing_quality = clamp01(
        (std::max(0.0, w[0]) * f_.candle_strength
         + std::max(0.0, w[1]) * f_.continuation
         + std::max(0.0, w[2]) * (1.0 - f_.exhaustion)
         + std::max(0.0, w[3]) * soft01(std::abs(f_.pressure_delta), 1.0)
         + std::max(0.0, w[4]) * f_.acceptance
         + std::max(0.0, w[5]) * f_.reclaim
         + std::max(0.0, w[6]) * (1.0 - f_.failed_breakout))
        / wsum);
}

}  // namespace mr
