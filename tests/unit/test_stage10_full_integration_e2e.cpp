#include <gtest/gtest.h>

#include "mr/market_core/pipeline.hpp"
#include "mr/market_core/model_bundle_store.hpp"
#include "mr/execution_engine/order_gateway.hpp"
#include "mr/prediction_engine/prediction.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/decision/trade_decision.hpp"
#include "mr/memory_engine/episode_recorder.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace mr;

namespace {

TradeIntent ready_long_intent() {
    TradeIntent intent;
    intent.id = 101;
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

/** Paper-only gateway — never sends LIVE Capital money orders. */
class PaperRecordingGateway final : public OrderGateway {
public:
    CapitalOrderResponse create_position(const CapitalOrderRequest& request) override {
        creates.push_back(request);
        CapitalOrderResponse r;
        r.success = healthy_;
        if (!healthy_) {
            r.error_message = "unhealthy";
            return r;
        }
        r.deal_id = "PAPER-" + std::to_string(++seq);
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

TEST(Stage10FullIntegrationE2E, MarketBrainDecisionRiskExecutionPositionExitMemory) {
    PaperRecordingGateway gw;
    MarketCorePipeline pipeline;
    pipeline.set_operating_mode(OperatingMode::Paper);
    pipeline.bind_order_gateway(gw);
    pipeline.set_account_equity(50'000.0);
    pipeline.set_broker_healthy(true);

    EpisodeRecorder recorder;
    recorder.set_model_id("stage10-e2e");
    recorder.set_config_hash("cfg-stage10");
    recorder.set_weight_hashes(PredictionWeightConfig::defaults(), DecisionWeightConfig::defaults(),
                               RiskWeightConfig::defaults(), ExecutionWeightConfig::defaults(),
                               PositionWeightConfig::defaults());
    recorder.begin_episode("ep-stage10-e2e", 1);
    pipeline.attach_episode_recorder(&recorder);

    ASSERT_TRUE(pipeline.has_execution());
    ASSERT_TRUE(pipeline.broker_healthy());

    // Decision → Risk → Execution → Position (paper fill; no LIVE money)
    ASSERT_TRUE(pipeline.enter_from_decision(ready_long_intent(), strong_long_dual(), 2000.0, 0.2));
    ASSERT_EQ(pipeline.open_positions().size(), 1u);
    ASSERT_EQ(gw.creates.size(), 1u);
    EXPECT_EQ(gw.creates.front().client_order_id, "vs2-101");
    EXPECT_EQ(pipeline.open_positions().front().deal_id, "PAPER-1");

    const auto snap_entry = pipeline.brain_snapshot();
    const auto& ctx_entry = snap_entry.instruments.at(1);
    EXPECT_TRUE(ctx_entry.has_risk);
    EXPECT_TRUE(ctx_entry.risk.approved);
    EXPECT_TRUE(ctx_entry.has_execution);
    EXPECT_EQ(ctx_entry.execution.status, ExecutionStatus::Filled);
    EXPECT_TRUE(ctx_entry.has_position);
    EXPECT_EQ(ctx_entry.position_decision.action, PositionAction::Hold);

    PriceDynamics pd;
    pd.velocity = 0.01;
    pipeline.update_open_positions(1, strong_long_dual(), pd, 2010.0);
    EXPECT_EQ(pipeline.open_positions().size(), 1u);

    // Thesis failure → Exit through ExecutionEngine.close (still paper)
    pipeline.update_open_positions(1, invalidated_long_dual(), pd, 1998.0);
    EXPECT_EQ(gw.closes.size(), 1u);
    EXPECT_TRUE(pipeline.open_positions().empty());

    const auto& ctx_exit = pipeline.brain_snapshot().instruments.at(1);
    EXPECT_EQ(ctx_exit.position_decision.action, PositionAction::Exit);
    EXPECT_EQ(ctx_exit.execution.status, ExecutionStatus::Filled);

    recorder.seal_episode();
    const auto ep = recorder.take_episode();
    EXPECT_TRUE(ep.sealed);
    EXPECT_EQ(ep.episode_id, "ep-stage10-e2e");
}

TEST(Stage10FullIntegrationE2E, BrokerUnhealthyFailClosedBlocksFilledEntry) {
    PaperRecordingGateway gw;
    gw.healthy_ = false;
    MarketCorePipeline pipeline;
    pipeline.bind_order_gateway(gw);
    pipeline.set_account_equity(50'000.0);
    pipeline.set_broker_healthy(false);

    EXPECT_FALSE(pipeline.broker_healthy());
    (void)pipeline.enter_from_decision(ready_long_intent(), strong_long_dual(), 2000.0, 0.2);
    EXPECT_TRUE(pipeline.open_positions().empty());
}

TEST(Stage10FullIntegrationE2E, HydrateOpenPositionsRecoveryWithoutNewOrders) {
    PaperRecordingGateway gw;
    MarketCorePipeline pipeline;
    pipeline.bind_order_gateway(gw);
    pipeline.set_account_equity(50'000.0);
    pipeline.set_broker_healthy(true);

    PositionState recovered;
    recovered.instrument = 1;
    recovered.direction = Direction::Long;
    recovered.quantity = 1.5;
    recovered.entry_price = 2001.0;
    recovered.current_price = 2001.0;
    recovered.deal_id = "RECOVERED-1";

    pipeline.hydrate_open_positions({recovered});
    ASSERT_EQ(pipeline.open_positions().size(), 1u);
    EXPECT_EQ(pipeline.open_positions().front().deal_id, "RECOVERED-1");
    EXPECT_TRUE(gw.creates.empty());
}

TEST(Stage10FullIntegrationE2E, ModelBundleLoadAndRollback) {
    PaperRecordingGateway gw;
    MarketCorePipeline pipeline;
    pipeline.bind_order_gateway(gw);
    pipeline.set_account_equity(50'000.0);

    const auto dir = std::filesystem::temp_directory_path() / "vs_v2_stage10_models";
    std::filesystem::create_directories(dir);
    const auto path_a = dir / "model_a.json";
    const auto path_b = dir / "model_b.json";
    {
        std::ofstream out(path_a);
        out << R"({"model_id":"a","model_version":"1.0.0","weights":{}})";
    }
    {
        std::ofstream out(path_b);
        out << R"({"model_id":"b","model_version":"2.0.0","weights":{}})";
    }

    ModelBundleStore store;
    ASSERT_TRUE(store.load_file(path_a.string(), pipeline, "a", "1.0.0"));
    EXPECT_EQ(store.model_id(), "a");
    EXPECT_EQ(store.model_version(), "1.0.0");
    ASSERT_TRUE(store.load_file(path_b.string(), pipeline, "b", "2.0.0"));
    EXPECT_EQ(store.model_id(), "b");
    ASSERT_TRUE(store.has_previous());
    ASSERT_TRUE(store.rollback(pipeline));
    EXPECT_EQ(store.model_id(), "a");
    EXPECT_EQ(store.model_version(), "1.0.0");
}

TEST(Stage10FullIntegrationE2E, ExecutionClientOrderIdIsStableIdempotencyKey) {
    PaperRecordingGateway gw;
    MarketCorePipeline pipeline;
    pipeline.bind_order_gateway(gw);
    pipeline.set_account_equity(50'000.0);
    pipeline.set_broker_healthy(true);

    ASSERT_TRUE(pipeline.enter_from_decision(ready_long_intent(), strong_long_dual(), 2000.0, 0.2));
    ASSERT_EQ(gw.creates.size(), 1u);
    EXPECT_EQ(gw.creates.front().client_order_id, "vs2-101");
}
