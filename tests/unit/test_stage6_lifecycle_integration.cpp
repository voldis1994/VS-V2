#include <gtest/gtest.h>

#include "mr/market_core/pipeline.hpp"
#include "mr/execution_engine/order_gateway.hpp"
#include "mr/prediction_engine/prediction.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/decision/trade_decision.hpp"

#include <string>
#include <vector>

using namespace mr;

namespace {

TradeIntent ready_long_intent() {
    TradeIntent intent;
    intent.id = 42;
    intent.instrument = 1;
    intent.direction = Direction::Long;
    intent.created_at = Timestamp{10'000'000'000};
    intent.reference_price = 2000.0;
    intent.probability = 0.8;
    intent.expected_value = 2.0;
    intent.stop_loss = 1990.0;
    intent.take_profit = 2040.0;
    intent.decision = EntryDecision::EntryReady;
    return intent;
}

DualPrediction strong_long_dual() {
    DualPrediction d;
    d.has_structure_authority = true;
    d.has_micro_authority = true;
    d.evidence_sufficient = true;
    d.long_side.direction = Direction::Long;
    d.long_side.continuation = 0.9;
    d.long_side.reversal_failure = 0.05;
    d.long_side.expected_move = 1.0;
    d.long_side.adverse_move = 0.2;
    d.long_side.probability = 0.8;
    d.long_side.confidence = 0.85;
    d.long_side.expected_value = 1.5;
    d.long_side.invalidation = 0.05;
    d.long_side.thesis_quality = 0.9;
    d.short_side.direction = Direction::Short;
    d.short_side.continuation = 0.1;
    d.short_side.thesis_quality = 0.1;
    d.short_side.expected_value = -0.5;
    d.short_side.invalidation = 0.7;
    return d;
}

DualPrediction invalidated_long_dual() {
    DualPrediction d = strong_long_dual();
    d.long_side.continuation = 0.05;
    d.long_side.reversal_failure = 0.95;
    d.long_side.invalidation = 0.95;
    d.long_side.thesis_quality = 0.05;
    d.long_side.expected_value = -1.0;
    d.long_side.adverse_move = 1.0;
    return d;
}

class MockGateway final : public OrderGateway {
public:
    CapitalOrderResponse create_position(const CapitalOrderRequest& request) override {
        creates.push_back(request);
        CapitalOrderResponse r;
        r.success = healthy_;
        if (!healthy_) {
            r.error_message = "unhealthy";
            return r;
        }
        r.deal_id = "DEAL-" + std::to_string(++seq);
        r.fill_price = request.price;
        r.filled_quantity = request.quantity;
        return r;
    }

    CapitalOrderResponse close_position(const std::string& deal_id) override {
        closes.push_back(deal_id);
        CapitalOrderResponse r;
        r.success = healthy_;
        r.deal_id = deal_id;
        if (!healthy_) r.error_message = "unhealthy";
        return r;
    }

    [[nodiscard]] bool healthy() const override { return healthy_; }

    bool healthy_{true};
    int seq{0};
    std::vector<CapitalOrderRequest> creates;
    std::vector<std::string> closes;
};

}  // namespace

TEST(Stage6LifecycleIntegration, DecisionRiskExecutionFillOpensPositionInBrainState) {
    MockGateway gw;
    MarketCorePipeline pipeline;
    pipeline.bind_order_gateway(gw);
    pipeline.set_account_equity(50'000.0);

    ASSERT_TRUE(pipeline.has_execution());
    ASSERT_TRUE(pipeline.enter_from_decision(ready_long_intent(), strong_long_dual(), 2000.0, 0.2));
    ASSERT_EQ(pipeline.open_positions().size(), 1u);
    EXPECT_EQ(pipeline.open_positions().front().deal_id, "DEAL-1");
    EXPECT_GT(pipeline.open_positions().front().quantity, 0.0);
    EXPECT_EQ(gw.creates.size(), 1u);

    const auto snap = pipeline.brain_snapshot();
    const auto& ctx = snap.instruments.at(1);
    EXPECT_TRUE(ctx.has_risk);
    EXPECT_TRUE(ctx.risk.approved);
    EXPECT_TRUE(ctx.has_execution);
    EXPECT_EQ(ctx.execution.status, ExecutionStatus::Filled);
    EXPECT_TRUE(ctx.has_position);
    EXPECT_EQ(ctx.position_decision.action, PositionAction::Hold);
    EXPECT_EQ(ctx.position.deal_id, "DEAL-1");
}

TEST(Stage6LifecycleIntegration, HoldProtectThenExitThroughExecution) {
    MockGateway gw;
    MarketCorePipeline pipeline;
    pipeline.bind_order_gateway(gw);
    pipeline.set_account_equity(50'000.0);
    ASSERT_TRUE(pipeline.enter_from_decision(ready_long_intent(), strong_long_dual(), 2000.0, 0.2));

    PriceDynamics pd;
    pd.velocity = 0.01;
    pipeline.update_open_positions(1, strong_long_dual(), pd, 2001.0);
    {
        const auto& ctx = pipeline.brain_snapshot().instruments.at(1);
        EXPECT_TRUE(ctx.has_position);
        EXPECT_EQ(ctx.position_decision.action, PositionAction::Hold);
        EXPECT_TRUE(gw.closes.empty());
    }

    // Build MFE then give back — expect PROTECT (stop tighten), no close yet.
    pipeline.update_open_positions(1, strong_long_dual(), pd, 2010.0);
    auto soft = strong_long_dual();
    soft.long_side.continuation = 0.55;
    soft.long_side.thesis_quality = 0.55;
    pipeline.update_open_positions(1, soft, pd, 2002.0);
    {
        const auto& ctx = pipeline.brain_snapshot().instruments.at(1);
        EXPECT_EQ(ctx.position_decision.action, PositionAction::Protect);
        EXPECT_GE(ctx.position.stop_loss, 2000.0);
        EXPECT_TRUE(gw.closes.empty());
        EXPECT_EQ(pipeline.open_positions().size(), 1u);
    }

    // Thesis invalidation → EXIT through ExecutionEngine.close
    pipeline.update_open_positions(1, invalidated_long_dual(), pd, 1998.0);
    EXPECT_EQ(gw.closes.size(), 1u);
    EXPECT_TRUE(pipeline.open_positions().empty());
    const auto& ctx = pipeline.brain_snapshot().instruments.at(1);
    EXPECT_EQ(ctx.position_decision.action, PositionAction::Exit);
    EXPECT_EQ(ctx.execution.status, ExecutionStatus::Filled);
    EXPECT_EQ(ctx.execution.explanation, "closed");
}

TEST(Stage6LifecycleIntegration, ReduceRoutesThroughExecutionEngine) {
    MockGateway gw;
    MarketCorePipeline pipeline;
    pipeline.bind_order_gateway(gw);
    pipeline.set_account_equity(50'000.0);

    // Bias PositionBrain toward REDUCE after MFE + thesis drop.
    auto cfg = PositionWeightConfig::defaults();
    cfg.w_dynamics = 0.0;
    cfg.w_peak_retention = 0.1;
    cfg.protect_scale = 2.0;
    cfg.reduce_scale = 0.5;
    pipeline.position_brain().set_weight_config(cfg);

    ASSERT_TRUE(pipeline.enter_from_decision(ready_long_intent(), strong_long_dual(), 2000.0, 0.2));
    const double entry_qty = pipeline.open_positions().front().quantity;

    pipeline.update_open_positions(1, strong_long_dual(), {}, 2008.0);

    DualPrediction mid = strong_long_dual();
    mid.long_side.continuation = 0.2;
    mid.long_side.thesis_quality = 0.15;
    mid.long_side.reversal_failure = 0.65;
    mid.long_side.invalidation = 0.4;
    mid.long_side.expected_value = 0.05;
    pipeline.update_open_positions(1, mid, {}, 2007.5);

    const auto& ctx = pipeline.brain_snapshot().instruments.at(1);
    EXPECT_EQ(ctx.position_decision.action, PositionAction::Reduce);
    // Opposite-side create_position used for reduce (entry create + reduce create).
    EXPECT_GE(gw.creates.size(), 2u);
    EXPECT_EQ(gw.creates.back().direction, Direction::Short);
    EXPECT_LT(pipeline.open_positions().front().quantity, entry_qty);
    EXPECT_EQ(ctx.execution.explanation, "reduced");
}

TEST(Stage6LifecycleIntegration, WithoutGatewayPendingOnlyNoSecondBrain) {
    MarketCorePipeline pipeline;
    pipeline.set_account_equity(50'000.0);
    EXPECT_FALSE(pipeline.has_execution());
    EXPECT_TRUE(pipeline.enter_from_decision(ready_long_intent(), strong_long_dual(), 2000.0, 0.2));
    EXPECT_TRUE(pipeline.open_positions().empty());
    EXPECT_EQ(pipeline.pending_intents().size(), 1u);
    EXPECT_TRUE(pipeline.brain_snapshot().instruments.at(1).has_risk);
}

TEST(Stage6LifecycleIntegration, SingleCandleAgainstDoesNotForceExit) {
    MockGateway gw;
    MarketCorePipeline pipeline;
    pipeline.bind_order_gateway(gw);
    pipeline.set_account_equity(50'000.0);
    ASSERT_TRUE(pipeline.enter_from_decision(ready_long_intent(), strong_long_dual(), 2000.0, 0.2));

    PriceDynamics pd;
    pd.velocity = -1.0;  // one candle strongly against
    pipeline.update_open_positions(1, strong_long_dual(), pd, 1999.5);

    EXPECT_TRUE(gw.closes.empty());
    EXPECT_EQ(pipeline.open_positions().size(), 1u);
    EXPECT_NE(pipeline.brain_snapshot().instruments.at(1).position_decision.action,
              PositionAction::Exit);
}
