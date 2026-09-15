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

TEST(RuntimeModeSwitch, ShadowBlocksNewEntries) {
    MockGateway gw;
    MarketCorePipeline pipeline;
    pipeline.bind_order_gateway(gw);
    pipeline.set_account_equity(50'000.0);
    pipeline.set_operating_mode(OperatingMode::Shadow);

    EXPECT_FALSE(pipeline.enter_from_decision(ready_long_intent(), strong_long_dual(), 2000.0, 0.2));
    EXPECT_TRUE(pipeline.open_positions().empty());
    EXPECT_TRUE(gw.creates.empty());
}

TEST(RuntimeModeSwitch, LiveToShadowKeepsManageExit) {
    MockGateway gw;
    MarketCorePipeline pipeline;
    pipeline.bind_order_gateway(gw);
    pipeline.set_account_equity(50'000.0);
    pipeline.set_operating_mode(OperatingMode::Live);

    ASSERT_TRUE(pipeline.enter_from_decision(ready_long_intent(), strong_long_dual(), 2000.0, 0.2));
    ASSERT_EQ(pipeline.open_positions().size(), 1u);
    EXPECT_EQ(gw.creates.size(), 1u);

    // LIVE→SHADOW: block new entries; open positions keep manage/exit.
    pipeline.set_operating_mode(OperatingMode::Shadow);
    EXPECT_FALSE(pipeline.enter_from_decision(ready_long_intent(), strong_long_dual(), 2000.0, 0.2));
    EXPECT_EQ(gw.creates.size(), 1u);

    PriceDynamics pd;
    pd.velocity = -0.01;
    pipeline.update_open_positions(1, invalidated_long_dual(), pd, 1998.0);
    EXPECT_EQ(gw.closes.size(), 1u);
    EXPECT_TRUE(pipeline.open_positions().empty());
}

TEST(RuntimeModeSwitch, ShadowToLiveAllowsEntriesAgain) {
    MockGateway gw;
    MarketCorePipeline pipeline;
    pipeline.bind_order_gateway(gw);
    pipeline.set_account_equity(50'000.0);
    pipeline.set_operating_mode(OperatingMode::Shadow);
    EXPECT_FALSE(pipeline.enter_from_decision(ready_long_intent(), strong_long_dual(), 2000.0, 0.2));

    pipeline.set_operating_mode(OperatingMode::Live);
    EXPECT_TRUE(pipeline.enter_from_decision(ready_long_intent(), strong_long_dual(), 2000.0, 0.2));
    EXPECT_EQ(pipeline.open_positions().size(), 1u);
}

TEST(RuntimeModeSwitch, PaperWithoutLiveGatewayDoesNotCreateBrokerOrders) {
    // PAPER deploy path leaves CapitalOrderGateway unbound; pending only, no broker creates.
    MockGateway gw;
    MarketCorePipeline pipeline;
    pipeline.set_account_equity(50'000.0);
    pipeline.set_operating_mode(OperatingMode::Paper);
    EXPECT_FALSE(pipeline.has_execution());

    EXPECT_TRUE(pipeline.enter_from_decision(ready_long_intent(), strong_long_dual(), 2000.0, 0.2));
    EXPECT_TRUE(pipeline.open_positions().empty());
    EXPECT_EQ(pipeline.drain_pending_intents().size(), 1u);
    EXPECT_TRUE(gw.creates.empty());
}

TEST(RuntimeModeSwitch, ReplayClearsPriorLiveGateway) {
    MockGateway gw;
    MarketCorePipeline pipeline;
    pipeline.set_operating_mode(OperatingMode::Live);
    pipeline.bind_order_gateway(gw);
    ASSERT_TRUE(pipeline.has_execution());

    // Switching to Replay drops the prior LIVE gateway (no broker orders on replay path).
    pipeline.set_operating_mode(OperatingMode::Replay);
    EXPECT_FALSE(pipeline.has_execution());
    EXPECT_TRUE(gw.creates.empty());
}
