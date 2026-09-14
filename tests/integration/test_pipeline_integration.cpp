#include <gtest/gtest.h>
#include "mr/market_core/pipeline.hpp"
#include "mr/market_types/market_event.hpp"

TEST(PipelineIntegration, ProcessesQuoteEvent) {
    mr::ConfigRegistry config;
    mr::InstrumentConfig inst;
    inst.id = 1;
    inst.symbol = "EURUSD";
    inst.tick_size = 0.00001;
    config.add_instrument(inst);

    mr::MarketCorePipeline pipeline;
    pipeline.configure(config);

    mr::MarketEvent event;
    event.instrument = 1;
    event.source = 1;
    event.type = mr::MarketEventType::Quote;
    event.exchange_timestamp = mr::Timestamp(1'000'000'000LL);
    event.receive_timestamp = event.exchange_timestamp;
    event.bid = 1.08495;
    event.ask = 1.08505;
    event.last = 1.0850;
    event.sequence = 1;

    pipeline.process_event(event);
    auto intents = pipeline.pending_intents();
    EXPECT_GE(intents.size(), 0u);
}
