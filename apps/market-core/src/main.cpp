#include "mr/market_core/runtime.hpp"
#include "mr/brain/brain_version.hpp"
#include "mr/capital/capital_client.hpp"
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
std::atomic<bool> g_running{true};
void on_signal(int) { g_running = false; }
}  // namespace

int main(int argc, char** argv) {
    std::string mode = "PAPER";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--mode" || arg == "-m") && i + 1 < argc) {
            mode = argv[++i];
        }
    }

    mr::MarketCoreRuntime runtime;
    mr::ConfigRegistry config;
    const auto runtime_mode = mr::parse_runtime_mode(mode);
    runtime.configure(config, runtime_mode);

    std::cout << "VS-V2 market-core ready mode=" << mode
              << " brain=" << mr::kBrainVersion << std::endl;

    if (runtime_mode == mr::RuntimeMode::Live || runtime_mode == mr::RuntimeMode::Demo) {
        const char* api_key = std::getenv("CAPITAL_API_KEY");
        const char* password = std::getenv("CAPITAL_PASSWORD");
        const char* identifier = std::getenv("CAPITAL_IDENTIFIER");
        const char* epic = std::getenv("CAPITAL_EPIC");
        const char* base = std::getenv("CAPITAL_BASE_URL");
        if (!api_key || !password || !identifier || !epic) {
            std::cerr << "LIVE/DEMO requires CAPITAL_API_KEY, CAPITAL_PASSWORD, "
                         "CAPITAL_IDENTIFIER, CAPITAL_EPIC\n";
            return 2;
        }

        const std::string base_url =
            base ? base
                 : (runtime_mode == mr::RuntimeMode::Live
                        ? "https://api-capital.backend-capital.com"
                        : "https://demo-api-capital.backend-capital.com");

        mr::CapitalClient client(base_url);
        client.connect();
        if (!client.authenticate(api_key, password, identifier)) {
            std::cerr << "Capital authenticate failed status=" << client.last_http_status()
                      << std::endl;
            return 3;
        }
        client.instruments().set(1, epic);

        mr::LiveFeedConfig feed;
        feed.instrument = 1;
        feed.source = 1;
        feed.epic = epic;

        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);
        std::cout << "VS-V2 market-core live multi-clock path starting epic=" << epic
                  << std::endl;
        runtime.run_live(client, feed, g_running);
        std::cout << "VS-V2 market-core live path stopped" << std::endl;
        return 0;
    }

    // PAPER/REPLAY: process stays ready without inventing a second trading brain.
    return 0;
}
