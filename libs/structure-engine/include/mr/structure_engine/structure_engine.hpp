#pragma once

#include "mr/structure_engine/structure_features.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/market_types/candle.hpp"
#include "mr/market_types/timeframe.hpp"
#include <array>
#include <cstdint>
#include <deque>
#include <vector>

namespace mr {

/**
 * Authoritative Market Structure Brain.
 *
 * Structure mutates ONLY from Capital CLOSED 1m+ OHLC.
 * RAW quotes, forming candles, and 10s closes must never call this path
 * for broad structure (caller contract + sub-minute refuse).
 *
 * Structure is derived from multi-candle / multi-swing sequences, not a
 * single bar. Per-TF lanes (1m/5m/15m/1h) feed one hierarchical snapshot.
 */
class StructureEngine {
public:
    /** Update structure from an authority closed candle (Capital 1m+). */
    void on_authority_close(const Candle& closed, Timeframe tf, const PriceDynamics& pd);

    /** Hierarchical composite (1m primary + HTF overlay). */
    [[nodiscard]] StructureFeatures snapshot() const { return composite_; }

    /** Per-timeframe lane snapshot (empty features if lane unused). */
    [[nodiscard]] StructureFeatures snapshot(Timeframe tf) const;

    [[nodiscard]] bool has_authority() const { return has_authority_; }
    [[nodiscard]] Timeframe authority_timeframe() const { return authority_tf_; }
    [[nodiscard]] std::uint32_t authority_updates() const { return authority_updates_; }

    void reset();

private:
    static constexpr std::size_t kMaxBars = 256;
    static constexpr int kPivotLeft = 1;
    static constexpr int kPivotRight = 1;

    enum class PivotKind : std::uint8_t { High = 0, Low = 1 };

    struct SwingPoint {
        PivotKind kind{PivotKind::High};
        double price{0};
        std::size_t bar_index{0};
        SwingLabel label{SwingLabel::None};
    };

    struct TfLane {
        std::deque<Candle> bars;
        std::vector<SwingPoint> swings;
        StructureFeatures features{};
        TrendBias prev_trend{TrendBias::Unknown};
        std::uint32_t bars_in_trend{0};
        bool breakout_armed_up{false};
        bool breakout_armed_down{false};
        bool has_data{false};
    };

    static int lane_index(Timeframe tf);
    static bool is_authority_tf(Timeframe tf);

    void recompute_lane(TfLane& lane, Timeframe tf);
    void detect_new_swings(TfLane& lane);
    void rebuild_all_swings(TfLane& lane);
    void classify_trend(TfLane& lane);
    void classify_range_vol_breakout(TfLane& lane);
    void classify_pullback_reversal(TfLane& lane);
    void rebuild_composite();

    std::array<TfLane, 4> lanes_{};  // 1m, 5m, 15m, 1h
    StructureFeatures composite_{};
    bool has_authority_{false};
    Timeframe authority_tf_{Timeframe::Minute1};
    std::uint32_t authority_updates_{0};
};

}  // namespace mr
