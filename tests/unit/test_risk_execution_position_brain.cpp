#include <gtest/gtest.h>

#include "mr/risk/risk_engine.hpp"
#include "mr/execution_engine/execution_engine.hpp"
#include "mr/execution_engine/order_gateway.hpp"
#include "mr/position_brain/position_brain.hpp"
#include "mr/prediction_engine/prediction.hpp"
#include "mr/perception_engine/price_dynamics.hpp"
#include "mr/brain/brain_state.hpp"
#include "mr/decision/trade_decision.hpp"

#include <string>
#include <vector>

using namespace mr;

namespace {

TradeIntent ready_long_intent() {
    TradeIntent intent;
    intent.id = 7;
    intent.instrument = 1;
    intent.direction = Direction::Long;
    intent.created_at = Timestamp{5'000'000'000};
    intent.reference_price = 2000.0;
    intent.probability = 0.72;
    intent.expected_value = 1.5;
    intent.stop_loss = 1990.0;
    intent.take_profit = 2020.0;
    intent.decision = EntryDecision::EntryReady;
    return intent;
}

SidePrediction strong_long_thesis() {
    SidePrediction s;
    s.direction = Direction::Long;
    s.continuation = 0.85;
    s.reversal_failure = 0.05;
    s.expected_move = 0.8;
    s.adverse_move = 0.2;
    s.probability = 0.75;
    s.confidence = 0.8;
    s.expected_value = 1.2;
    s.invalidation = 0.05;
    s.thesis_quality = 0.85;
    s.uncertainty = 0.15;
    return s;
}

SidePrediction invalidated_long_thesis() {
    SidePrediction s = strong_long_thesis();
    s.continuation = 0.05;
    s.reversal_failure = 0.9;
    s.invalidation = 0.95;
    s.thesis_quality = 0.05;
    s.expected_value = -0.8;
    s.adverse_move = 0.9;
    return s;
}

class MockGateway final : public OrderGateway {
public:
    CapitalOrderResponse create_position(const CapitalOrderRequest& request) override {
        create_calls.push_back(request);
        CapitalOrderResponse r;
        if (!healthy_flag_ || force_reject_) {
            r.success = false;
            r.error_message = force_reject_ ? "broker_reject" : "unhealthy";
            return r;
        }
        if (fail_times_ > 0) {
            --fail_times_;
            r.success = false;
            r.error_message = "transient";
            return r;
        }
        r.success = true;
        r.deal_id = "DEAL-" + std::to_string(++deal_seq_);
        r.fill_price = request.price;
        r.filled_quantity = request.quantity;
        return r;
    }

    CapitalOrderResponse close_position(const std::string& deal_id) override {
        close_calls.push_back(deal_id);
        CapitalOrderResponse r;
        r.success = healthy_flag_;
        r.deal_id = deal_id;
        if (!healthy_flag_) r.error_message = "unhealthy";
        return r;
    }

    [[nodiscard]] bool healthy() const override { return healthy_flag_; }

    bool healthy_flag_{true};
    bool force_reject_{false};
    int fail_times_{0};
    std::vector<CapitalOrderRequest> create_calls;
    std::vector<std::string> close_calls;

private:
    int deal_seq_{0};
};

RiskRequest base_risk_request(const TradeIntent& intent) {
    RiskRequest req;
    req.intent = intent;
    req.mid_price = intent.reference_price;
    req.account_equity = 25'000.0;
    req.spread = 0.2;
    req.spread_cost = 0.2;
    req.broker_healthy = true;
    req.data_fresh = true;
    return req;
}

}  // namespace

TEST(RiskEngineStage6, MissingEquityIsFailClosed) {
    RiskEngine risk;
    auto req = base_risk_request(ready_long_intent());
    req.account_equity = 0;
    auto d = risk.evaluate(req);
    EXPECT_FALSE(d.approved);
    ASSERT_FALSE(d.reason_codes.empty());
    EXPECT_EQ(d.reason_codes.front(), "MISSING_ACCOUNT_EQUITY");
}

TEST(RiskEngineStage6, ApprovesSizedEntryWithRealEquity) {
    RiskEngine risk;
    auto d = risk.evaluate(base_risk_request(ready_long_intent()));
    EXPECT_TRUE(d.approved);
    EXPECT_GT(d.approved_quantity, 0.0);
    EXPECT_EQ(d.type, RiskIntentType::Entry);
}

TEST(RiskEngineStage6, VetoesWideSpreadUsingRealSpread) {
    RiskEngine risk;
    auto req = base_risk_request(ready_long_intent());
    req.spread = 20.0;
    req.spread_cost = 20.0;
    auto d = risk.evaluate(req);
    EXPECT_FALSE(d.approved);
    ASSERT_FALSE(d.reason_codes.empty());
    EXPECT_EQ(d.reason_codes.front(), "SPREAD_TOO_WIDE");
}

TEST(RiskEngineStage6, VetoesStaleBrokerAndDailyLoss) {
    RiskEngine risk;
    {
        auto req = base_risk_request(ready_long_intent());
        req.data_fresh = false;
        auto d = risk.evaluate(req);
        EXPECT_FALSE(d.approved);
        EXPECT_EQ(d.reason_codes.front(), "STALE_DATA");
    }
    {
        auto req = base_risk_request(ready_long_intent());
        req.broker_healthy = false;
        auto d = risk.evaluate(req);
        EXPECT_FALSE(d.approved);
        EXPECT_EQ(d.reason_codes.front(), "BROKER_FAILURE");
    }
    {
        auto req = base_risk_request(ready_long_intent());
        req.daily_pnl = -2000.0;
        auto d = risk.evaluate(req);
        EXPECT_FALSE(d.approved);
        EXPECT_EQ(d.reason_codes.front(), "DAILY_LOSS");
    }
    {
        auto req = base_risk_request(ready_long_intent());
        req.quote_age_ms = 10'000;
        auto d = risk.evaluate(req);
        EXPECT_FALSE(d.approved);
        EXPECT_EQ(d.reason_codes.front(), "STALE_QUOTE");
    }
}

TEST(RiskEngineStage6, DuplicateOrderWindowVeto) {
    RiskEngine risk;
    auto intent = ready_long_intent();
    ASSERT_TRUE(risk.evaluate(base_risk_request(intent)).approved);
    risk.remember_order(intent.instrument, intent.direction, intent.created_at);
    auto second = risk.evaluate(base_risk_request(intent));
    EXPECT_FALSE(second.approved);
    EXPECT_EQ(second.reason_codes.front(), "DUPLICATE_ORDER");
}

TEST(RiskEngineStage6, NegativeNetEvAfterRealSpreadCost) {
    RiskEngine risk;
    auto intent = ready_long_intent();
    intent.expected_value = 0.05;
    auto req = base_risk_request(intent);
    req.spread_cost = 0.2;
    auto d = risk.evaluate(req);
    EXPECT_FALSE(d.approved);
    EXPECT_EQ(d.reason_codes.front(), "NEGATIVE_NET_EV");
}

TEST(ExecutionEngineStage6, SubmitFillLifecycle) {
    MockGateway gw;
    ExecutionEngine exec(gw);
    auto intent = ready_long_intent();
    auto rep = exec.submit(intent, 2.5);
    EXPECT_EQ(rep.status, ExecutionStatus::Filled);
    EXPECT_EQ(rep.filled_quantity, 2.5);
    EXPECT_EQ(rep.fill_price, intent.reference_price);
    EXPECT_FALSE(rep.deal_id.empty());
    EXPECT_EQ(gw.create_calls.size(), 1u);
    EXPECT_EQ(exec.fills().fills().size(), 1u);
}

TEST(ExecutionEngineStage6, RejectRetryThenFill) {
    MockGateway gw;
    gw.fail_times_ = 2;
    ExecutionWeightConfig cfg;
    cfg.max_attempts = 3;
    cfg.backoff_ms = 0;
    ExecutionEngine exec(gw, cfg);
    auto rep = exec.submit(ready_long_intent(), 1.0);
    EXPECT_EQ(rep.status, ExecutionStatus::Filled);
    EXPECT_EQ(rep.attempts, 3u);
    EXPECT_EQ(gw.create_calls.size(), 3u);
}

TEST(ExecutionEngineStage6, DuplicateAndHardReject) {
    MockGateway gw;
    ExecutionWeightConfig cfg;
    cfg.dedup_window_ms = 5'000;
    cfg.max_attempts = 1;
    ExecutionEngine exec(gw, cfg);
    auto intent = ready_long_intent();
    ASSERT_EQ(exec.submit(intent, 1.0).status, ExecutionStatus::Filled);

    auto dup = exec.submit(intent, 1.0);
    EXPECT_EQ(dup.status, ExecutionStatus::Rejected);
    ASSERT_FALSE(dup.reason_codes.empty());
    EXPECT_EQ(dup.reason_codes.front(), "DUPLICATE_ORDER");

    gw.force_reject_ = true;
    intent.created_at = Timestamp{20'000'000'000};
    auto rej = exec.submit(intent, 1.0);
    EXPECT_EQ(rej.status, ExecutionStatus::Rejected);
    EXPECT_EQ(rej.reason_codes.front(), "REJECTED");
}

TEST(ExecutionEngineStage6, NeverInventTradeDecision) {
    MockGateway gw;
    ExecutionEngine exec(gw);
    auto intent = ready_long_intent();
    intent.decision = EntryDecision::NoTrade;
    auto rep = exec.submit(intent, 1.0);
    EXPECT_EQ(rep.status, ExecutionStatus::Rejected);
    EXPECT_EQ(rep.reason_codes.front(), "INTENT_NOT_READY");
    EXPECT_TRUE(gw.create_calls.empty());
}

TEST(PositionBrainStage6, HoldWhenThesisIntact) {
    PositionBrain brain;
    auto intent = ready_long_intent();
    auto thesis = strong_long_thesis();
    auto pos = brain.open(intent, 2000.0, 1.0, thesis);
    pos.current_price = 2001.0;
    PriceDynamics pd;
    pd.velocity = 0.01;
    auto d = brain.evaluate(pos, thesis, pd);
    EXPECT_EQ(d.action, PositionAction::Hold);
    EXPECT_EQ(d.reason, ExitReason::None);
}

TEST(PositionBrainStage6, ProtectOnPeakGiveback) {
    PositionBrain brain;
    auto intent = ready_long_intent();
    auto thesis = strong_long_thesis();
    auto pos = brain.open(intent, 2000.0, 1.0, thesis);
    brain.update_excursions(pos, 2010.0);
    brain.update_excursions(pos, 2002.0);
    auto soft = thesis;
    soft.continuation = 0.55;
    soft.thesis_quality = 0.55;
    auto d = brain.evaluate(pos, soft, {});
    EXPECT_EQ(d.action, PositionAction::Protect);
    EXPECT_EQ(d.reason, ExitReason::PeakProtection);
    EXPECT_GE(d.suggested_stop, pos.entry_price);
}

TEST(PositionBrainStage6, ReduceOnDegradingThesisWithMfe) {
    PositionWeightConfig cfg = PositionWeightConfig::defaults();
    cfg.w_dynamics = 0.0;
    cfg.w_peak_retention = 0.1;  // de-emphasize protect path for this case
    cfg.protect_scale = 2.0;
    cfg.reduce_scale = 0.5;
    PositionBrain brain(cfg);
    auto intent = ready_long_intent();
    auto thesis = strong_long_thesis();
    auto pos = brain.open(intent, 2000.0, 1.0, thesis);
    brain.update_excursions(pos, 2008.0);
    // Keep most of MFE so protect (peak giveback) does not dominate.
    pos.current_price = 2007.5;
    SidePrediction mid = thesis;
    mid.continuation = 0.2;
    mid.thesis_quality = 0.15;
    mid.reversal_failure = 0.65;
    mid.invalidation = 0.4;
    mid.expected_value = 0.05;
    auto d = brain.evaluate(pos, mid, {});
    EXPECT_EQ(d.action, PositionAction::Reduce);
    EXPECT_GT(d.reduce_fraction, 0.0);
}

TEST(PositionBrainStage6, ExitOnThesisInvalidation) {
    PositionBrain brain;
    auto intent = ready_long_intent();
    auto thesis = strong_long_thesis();
    auto pos = brain.open(intent, 2000.0, 1.0, thesis);
    pos.current_price = 1998.0;
    auto d = brain.evaluate(pos, invalidated_long_thesis(), {});
    EXPECT_EQ(d.action, PositionAction::Exit);
    EXPECT_TRUE(d.reason == ExitReason::HardInvalidation
                || d.reason == ExitReason::ThesisFailure);
}

TEST(PositionBrainStage6, DoesNotExitOnSingleCandleAgainst) {
    PositionWeightConfig cfg = PositionWeightConfig::defaults();
    cfg.w_dynamics = 0.25;
    PositionBrain brain(cfg);
    auto intent = ready_long_intent();
    auto thesis = strong_long_thesis();
    auto pos = brain.open(intent, 2000.0, 1.0, thesis);
    pos.current_price = 1999.5;
    PriceDynamics pd;
    pd.velocity = -1.0;
    auto d = brain.evaluate(pos, thesis, pd);
    EXPECT_NE(d.action, PositionAction::Exit);
}

TEST(PositionBrainStage6, StopAndTargetManagement) {
    PositionBrain brain;
    auto intent = ready_long_intent();
    auto pos = brain.open(intent, 2000.0, 1.0, strong_long_thesis());
    pos.current_price = 1989.0;
    auto stop_d = brain.evaluate(pos, strong_long_thesis(), {});
    EXPECT_EQ(stop_d.action, PositionAction::Exit);
    EXPECT_EQ(stop_d.reason, ExitReason::HardInvalidation);

    pos = brain.open(intent, 2000.0, 1.0, strong_long_thesis());
    pos.current_price = 2021.0;
    auto tp_d = brain.evaluate(pos, strong_long_thesis(), {});
    EXPECT_EQ(tp_d.action, PositionAction::Exit);
    EXPECT_EQ(tp_d.reason, ExitReason::Target);
}

TEST(BrainStateStage6, HoldsRiskExecutionPositionSnapshots) {
    BrainState state;
    RiskDecision risk;
    risk.approved = true;
    risk.approved_quantity = 1.25;
    risk.type = RiskIntentType::Entry;
    state.apply_risk(1, risk, Timestamp{10});

    ExecutionReport exec;
    exec.status = ExecutionStatus::Filled;
    exec.deal_id = "D-1";
    exec.fill_price = 2000.0;
    state.apply_execution(1, exec, Timestamp{20});

    PositionState pos;
    pos.instrument = 1;
    pos.entry_price = 2000.0;
    PositionDecision pd;
    pd.action = PositionAction::Hold;
    state.apply_position(1, pos, pd, Timestamp{30});

    const auto& ctx = state.latest().instruments.at(1);
    EXPECT_TRUE(ctx.has_risk);
    EXPECT_TRUE(ctx.has_execution);
    EXPECT_TRUE(ctx.has_position);
    EXPECT_TRUE(ctx.risk.approved);
    EXPECT_EQ(ctx.execution.deal_id, "D-1");
    EXPECT_EQ(ctx.position_decision.action, PositionAction::Hold);

    BrainContext quote;
    quote.instrument = 1;
    quote.ts = Timestamp{40};
    state.update(quote);
    const auto& ctx2 = state.latest().instruments.at(1);
    EXPECT_TRUE(ctx2.has_risk);
    EXPECT_TRUE(ctx2.has_execution);
    EXPECT_TRUE(ctx2.has_position);
    EXPECT_EQ(ctx2.execution.deal_id, "D-1");
}
