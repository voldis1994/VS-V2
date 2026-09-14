#pragma once

#include "mr/microstructure_engine/microstructure_features.hpp"
#include "mr/microstructure_engine/micro_norm_config.hpp"
#include "mr/market_types/market_event.hpp"
#include "mr/market_types/candle.hpp"
#include "mr/market_types/timeframe.hpp"
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace mr {

/**
 * 10s Microstructure Brain — continuous evidence layer (not a trading brain).
 *
 * Authority = CLOSED 10s OHLC only (one-shot per bar).
 * Forming 10s / RAW quotes must never confirm setup and must never
 * rewrite 1m+ broad structure (structure lives in StructureEngine).
 *
 * Evidence is continuous normalized measurements from the CLOSED 10s
 * sequence. MicroNormConfig holds Stage-8-calibratable scales/windows —
 * not market-behavior trigger gates.
 */
class MicrostructureEngine {
public:
    explicit MicrostructureEngine(MicroNormConfig cfg = MicroNormConfig::defaults());

    /** RAW quote / trade book metrics — never confirms setup. */
    void update(const NormalizedEvent& e);

    /**
     * CLOSED 10s authority path. Processes each closed bar exactly once
     * (keyed by open_time).
     */
    void on_closed_10s(const Candle& closed);

    void set_norm_config(MicroNormConfig cfg);
    [[nodiscard]] const MicroNormConfig& norm_config() const { return cfg_; }

    [[nodiscard]] MicrostructureFeatures snapshot() const { return f_; }
    [[nodiscard]] bool has_authority() const { return f_.has_authority; }
    [[nodiscard]] std::uint32_t closed_10s_count() const { return f_.closed_10s_count; }
    [[nodiscard]] std::optional<Timestamp> last_closed_open_time() const {
        return last_open_time_;
    }

    void reset();

private:
    static constexpr std::size_t kMaxBars = 128;
    static constexpr int kPivotLeft = 1;
    static constexpr int kPivotRight = 1;

    enum class PivotKind : std::uint8_t { High = 0, Low = 1 };
    struct SwingPoint {
        PivotKind kind{PivotKind::High};
        double price{0};
        std::size_t bar_index{0};
        int label{0};  // 1=HH 2=HL 3=LH 4=LL
    };

    void detect_new_swings();
    void rebuild_all_swings();
    void recompute_from_sequence();

    MicroNormConfig cfg_{};
    MicrostructureFeatures f_{};
    std::deque<Candle> bars_;
    std::vector<SwingPoint> swings_;
    std::optional<Timestamp> last_open_time_{};
    double prev_breakout_up_{0};
    double prev_breakout_down_{0};
    std::uint64_t trade_count_{0};
    std::uint64_t quote_count_{0};
};

}  // namespace mr
