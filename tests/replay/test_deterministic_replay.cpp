#include <gtest/gtest.h>
#include "mr/market_core/pipeline.hpp"
#include "mr/persistence/raw_event_storage.hpp"
#include "mr/replay/replay_engine.hpp"
#include <filesystem>
#include <thread>
#include <chrono>

namespace {

void write_dataset(const std::string& path, int events) {
    mr::RawEventWriter writer(path);
    for (int i = 0; i < events; ++i) {
        mr::MarketEvent e;
        e.instrument = 1;
        e.source = 1 + (i % 3);
        e.receive_timestamp = mr::Timestamp(static_cast<long long>(i) * 1'000'000);
        e.exchange_timestamp = e.receive_timestamp;
        e.provider_timestamp = e.receive_timestamp;
        e.type = mr::MarketEventType::Quote;
        const double mid = 1.0850 + ((i % 40) - 20) * 0.00001;
        e.bid = mid - 0.00005;
        e.ask = mid + 0.00005;
        e.last = mid;
        e.sequence = static_cast<mr::SequenceNumber>(i + 1);
        writer.write(e);
    }
    writer.flush();
}

std::vector<std::uint64_t> run_once(const std::string& path) {
    mr::ConfigRegistry config;
    mr::InstrumentConfig inst;
    inst.id = 1;
    inst.symbol = "EURUSD";
    inst.tick_size = 0.00001;
    config.add_instrument(inst);

    mr::MarketCorePipeline pipeline;
    pipeline.configure(config);

    mr::ReplayEngine replay;
    EXPECT_TRUE(replay.load(path));
    replay.set_speed(mr::ReplaySpeed::Maximum);
    replay.start([&](const mr::MarketEvent& event) { pipeline.process_event(event); });
    while (replay.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::vector<std::uint64_t> ids;
    for (const auto& intent : pipeline.pending_intents()) ids.push_back(intent.id);
    return ids;
}

}  // namespace

TEST(DeterministicReplay, IdenticalIntentIdsAcrossRuns) {
    const std::string path = "/tmp/mr_det_replay.mrev";
    write_dataset(path, 200);

    auto baseline = run_once(path);
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(run_once(path), baseline) << "run " << i;
    }
    std::filesystem::remove(path);
}
