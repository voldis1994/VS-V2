#pragma once
#include "mr/risk/risk_weight_config.hpp"
#include "mr/risk/risk_limits.hpp"
#include "mr/risk/risk_state.hpp"
#include "mr/risk/risk_request.hpp"
#include "mr/risk/risk_decision.hpp"
#include "mr/risk/exposure_state.hpp"
#include "mr/decision/trade_decision.hpp"
#include "mr/position_brain/position_types.hpp"
#include <optional>
#include <string>
#include <unordered_map>

namespace mr {

struct SizingResult {
    double quantity{0};
    bool approved{false};
    std::string reason;
};

struct GuardResult {
    bool pass{true};
    std::string reason;
};

/** Sole pre-trade veto / size limiter. Never chooses BUY/SELL/WAIT. */
class RiskEngine {
public:
    explicit RiskEngine(RiskWeightConfig cfg = RiskWeightConfig::defaults());
    explicit RiskEngine(RiskLimits limits);

    void set_weight_config(RiskWeightConfig cfg);
    [[nodiscard]] const RiskWeightConfig& weight_config() const { return cfg_; }

    void set_emergency_stop(bool on) { emergency_stop_ = on; }
    [[nodiscard]] bool emergency_stop() const { return emergency_stop_; }

    void set_daily_pnl(double pnl) {
        daily_pnl_ = pnl;
        has_daily_pnl_ = true;
    }
    void set_exposure(const ExposureState& exp) {
        exposure_ = exp;
        has_exposure_ = true;
    }
    void set_max_drawdown(double dd) {
        max_drawdown_ = dd;
        has_max_drawdown_ = true;
    }
    void set_realized_pnl(double pnl) {
        realized_pnl_ = pnl;
        has_realized_pnl_ = true;
    }

    [[nodiscard]] std::optional<double> daily_pnl() const {
        return has_daily_pnl_ ? std::optional<double>{daily_pnl_} : std::nullopt;
    }
    [[nodiscard]] std::optional<ExposureState> exposure() const {
        return has_exposure_ ? std::optional<ExposureState>{exposure_} : std::nullopt;
    }
    [[nodiscard]] std::optional<double> max_drawdown() const {
        return has_max_drawdown_ ? std::optional<double>{max_drawdown_} : std::nullopt;
    }
    [[nodiscard]] std::optional<double> realized_pnl() const {
        return has_realized_pnl_ ? std::optional<double>{realized_pnl_} : std::nullopt;
    }

    void remember_order(InstrumentId instrument, Direction direction, Timestamp ts);

    [[nodiscard]] SizingResult size_position(const TradeIntent& intent,
                                             double equity,
                                             double price) const;
    [[nodiscard]] GuardResult pre_trade_check(const RiskRequest& request) const;
    GuardResult monitor_position(const PositionState& pos, double daily_pnl);

    RiskDecision evaluate(const RiskRequest& request);

private:
    RiskWeightConfig cfg_{};
    bool emergency_stop_{false};
    double daily_pnl_{0};
    bool has_daily_pnl_{false};
    ExposureState exposure_{};
    bool has_exposure_{false};
    double max_drawdown_{0};
    bool has_max_drawdown_{false};
    double realized_pnl_{0};
    bool has_realized_pnl_{false};
    std::unordered_map<std::uint64_t, Timestamp> recent_orders_{};

    [[nodiscard]] static std::uint64_t order_key(InstrumentId instrument, Direction direction);
};

}  // namespace mr
