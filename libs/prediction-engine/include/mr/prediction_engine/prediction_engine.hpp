#pragma once

#include "mr/prediction_engine/prediction.hpp"
#include "mr/prediction_engine/prediction_weight_config.hpp"
#include "mr/structure_engine/structure_features.hpp"
#include "mr/microstructure_engine/microstructure_features.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/scenario_engine/scenario.hpp"

namespace mr {

/**
 * Prediction Brain — continuous dual-side thesis evaluation.
 *
 * Inputs: Stage-3 structure authority + Stage-4 CLOSED 10s micro evidence.
 * Outputs: independent LONG/SHORT SidePrediction into DualPrediction.
 * Does not emit orders. Does not use 14 concepts as entry triggers.
 * Weights are Stage-8 calibratable (PredictionWeightConfig).
 */
class PredictionEngine {
public:
    explicit PredictionEngine(PredictionWeightConfig cfg = PredictionWeightConfig::defaults());

    void set_weight_config(PredictionWeightConfig cfg);
    [[nodiscard]] const PredictionWeightConfig& weight_config() const { return cfg_; }

    /**
     * Authoritative prediction path.
     * Structure + micro only. PriceDynamics is optional soft context (not RAW quote authority).
     */
    [[nodiscard]] DualPrediction evaluate(const StructureFeatures& structure,
                                          bool has_structure_authority,
                                          const MicrostructureFeatures& micro,
                                          const PriceDynamics& pd = {}) const;

    /**
     * Legacy scenario adapter — maps DualPrediction side into Prediction.
     * Kept for older call sites; not the Stage-5 authority path.
     */
    [[nodiscard]] Prediction predict(const Scenario& scenario,
                                     const PriceDynamics& pd,
                                     Direction dir) const;

private:
    [[nodiscard]] SidePrediction evaluate_side(Direction dir,
                                               const StructureFeatures& st,
                                               const MicrostructureFeatures& micro,
                                               const PriceDynamics& pd) const;

    PredictionWeightConfig cfg_{};
};

}  // namespace mr
