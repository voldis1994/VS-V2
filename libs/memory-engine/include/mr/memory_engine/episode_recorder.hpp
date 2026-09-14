#pragma once

#include "mr/memory_engine/episode_types.hpp"
#include "mr/brain/brain_context.hpp"
#include "mr/decision/decision_weight_config.hpp"
#include "mr/execution_engine/execution_weight_config.hpp"
#include "mr/position_brain/position_weight_config.hpp"
#include "mr/prediction_engine/prediction_weight_config.hpp"
#include "mr/risk/risk_weight_config.hpp"

#include <string>

namespace mr {

/** Capture-only episode recorder — evidence/history, never a trading brain. */
class EpisodeRecorder {
public:
    void set_provenance(EpisodeProvenance provenance);
    void set_model_id(std::string model_id);
    void set_config_hash(std::string config_hash);
    void set_weight_hashes(const PredictionWeightConfig& prediction,
                           const DecisionWeightConfig& decision,
                           const RiskWeightConfig& risk,
                           const ExecutionWeightConfig& execution,
                           const PositionWeightConfig& position);

    void begin_episode(std::string episode_id, InstrumentId instrument);

    void record_raw_quote(const MarketEvent& event);
    void record_closed_10s(const Candle& candle, Timestamp ts, bool one_shot);
    void record_authority_ohlc(const Candle& candle, Timeframe tf, Timestamp ts);

    void record_brain_frame(const BrainContext& ctx, EpisodeClockDomain trigger, Timestamp ts);
    void record_position_action(const PositionDecision& decision);

    void on_entry(const PositionState& pos, Timestamp ts);
    void on_mark(double mid, Timestamp ts);
    void on_exit(const PositionState& pos_at_exit,
                 const PositionDecision& exit_decision,
                 double exit_price,
                 Timestamp ts);
    void on_post_exit_mark(double mid, Timestamp ts);
    void seal_episode();

    [[nodiscard]] bool active() const { return active_; }
    [[nodiscard]] bool sealed() const { return episode_.sealed; }
    [[nodiscard]] const TradeEpisode& episode() const { return episode_; }
    TradeEpisode take_episode();

private:
    static std::string hash_bytes(const void* data, std::size_t n);
    template <typename T>
    static std::string hash_pod(const T& v) {
        return hash_bytes(&v, sizeof(T));
    }

    TradeEpisode episode_{};
    bool active_{false};
    bool in_post_exit_{false};
    double entry_price_{0};
    Direction entry_direction_{Direction::Flat};
    std::uint64_t seq_{0};
};

}  // namespace mr
