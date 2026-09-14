#include <gtest/gtest.h>
#include "mr/market_core/pipeline.hpp"
#include "mr/market_types/market_clock.hpp"
#include "mr/risk/risk_engine.hpp"

using namespace mr;

TEST(ClockArchitecture, StructureNotUpdatedOnRawQuote) {
    MarketCorePipeline pipeline;
    ConfigRegistry config;
    pipeline.configure(config);

    MarketEvent event;
    event.instrument = 1;
    event.source = 1;
    event.type = MarketEventType::Quote;
    event.exchange_timestamp = Timestamp(1'000'000'000LL);
    event.receive_timestamp = event.exchange_timestamp;
    event.bid = 2000.0;
    event.ask = 2000.2;
    event.last = 2000.1;
    event.sequence = 1;

    EXPECT_FALSE(pipeline.structure().has_authority());
    pipeline.process_event(event);
    EXPECT_FALSE(pipeline.structure().has_authority());
    EXPECT_TRUE(pipeline.pending_intents().empty());
}

TEST(ClockArchitecture, AuthorityOhlcUpdatesStructureAndRequiresEquity) {
    MarketCorePipeline pipeline;
    ConfigRegistry config;
    pipeline.configure(config);

    Candle c;
    c.instrument = 1;
    c.open_time = Timestamp(0);
    c.open = 2000; c.high = 2010; c.low = 1995; c.close = 2005;
    c.ticks = 10;
    c.status = CandleStatus::Closed;

    pipeline.process_authority_ohlc(c, Timeframe::Minute1);
    EXPECT_TRUE(pipeline.structure().has_authority());
    // Without equity, risk is fail-closed => no pending intents.
    EXPECT_TRUE(pipeline.pending_intents().empty());

    pipeline.set_account_equity(25000.0);
    // Drive another authority bar so decision+risk run with equity present.
    c.open_time = Timestamp(60'000'000'000LL);
    c.open = 2005; c.high = 2020; c.low = 2000; c.close = 2015;
    pipeline.process_authority_ohlc(c, Timeframe::Minute1);
    // May or may not produce EntryReady depending on heuristics, but must not crash
    // and must never invent equity.
    EXPECT_TRUE(pipeline.has_account_equity());
}

TEST(RiskFailClosed, MissingEquityRejected) {
    RiskEngine risk;
    IdGenerator ids;
    TradeIntent intent;
    intent.id = ids.generate();
    intent.instrument = 1;
    intent.direction = Direction::Long;
    intent.decision = EntryDecision::EntryReady;
    intent.reference_price = 2000;
    intent.stop_loss = 1990;
    intent.expected_value = 1.0;
    intent.probability = 0.7;

    RiskRequest req;
    req.intent = intent;
    req.mid_price = 2000;
    req.account_equity = 0;  // missing

    auto d = risk.evaluate(req);
    EXPECT_FALSE(d.approved);
    ASSERT_FALSE(d.reason_codes.empty());
    EXPECT_EQ(d.reason_codes.front(), "MISSING_ACCOUNT_EQUITY");
}

TEST(DecisionSides, EvaluatesLongShortAndWait) {
    IdGenerator ids;
    DecisionEngine decision(ids);
    PredictionEngine prediction;
    Scenario sc;
    sc.confidence = 0.1;  // force WAIT via low confidence
    PriceDynamics pd;
    auto sides = decision.evaluate_long_short_wait(sc, prediction, pd, 0.1);
    EXPECT_EQ(sides.final_action, TradeAction::Wait);
}
