#include "mr/market_core/runtime.hpp"
#include "mr/market_core/live_capital_bootstrap.hpp"
#include "mr/market_core/capital_env.hpp"
#include "mr/brain/brain_version.hpp"
#include "mr/capital/capital_client.hpp"
#include <atomic>
#include <cstdlib>
#include <csignal>
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

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    std::cout << "VS-V2 market-core ready mode=" << mode
              << " brain=" << mr::kBrainVersion << std::endl;

    if (runtime_mode == mr::RuntimeMode::Live || runtime_mode == mr::RuntimeMode::Demo) {
        const auto creds = mr::load_capital_credentials_from_env();
        if (!creds.complete()) {
            std::cerr << "LIVE/DEMO requires " << mr::kCapitalApiKeyEnv << ", "
                      << mr::kCapitalApiPasswordEnv << ", " << mr::kCapitalIdentifierEnv
                      << ", " << mr::kCapitalEpicEnv << "\n";
            return 2;
        }

        const std::string base_url =
            !creds.base_url.empty()
                ? creds.base_url
                : (runtime_mode == mr::RuntimeMode::Live
                       ? "https://api-capital.backend-capital.com"
                       : "https://demo-api-capital.backend-capital.com");

        mr::CapitalClient client(base_url);
        client.connect();
        if (!client.authenticate(creds.api_key, creds.api_password, creds.identifier)) {
            std::cerr << "Capital authenticate failed status=" << client.last_http_status()
                      << std::endl;
            return 3;
        }
        client.instruments().set(1, creds.epic);

        mr::LiveFeedConfig feed;
        feed.instrument = 1;
        feed.source = 1;
        feed.epic = creds.epic;

        mr::LiveCapitalBootstrapConfig boot;
        boot.instrument = 1;
        boot.epic = creds.epic;
        boot.operating_mode = (runtime_mode == mr::RuntimeMode::Live)
                                  ? mr::OperatingMode::Live
                                  : mr::OperatingMode::Demo;
        boot.enable_execution = true;
        if (const char* mp = std::getenv("VS_V2_MODEL_PATH")) {
            boot.model_path = mp;
        }
        if (const char* mid = std::getenv("VS_V2_MODEL_ID")) {
            boot.model_id = mid;
        }
        if (const char* mv = std::getenv("VS_V2_MODEL_VERSION")) {
            boot.model_version = mv;
        }

        mr::LiveCapitalBootstrap bootstrap(runtime, client);
                if (!bootstrap.prepare(boot)) {
            if (!boot.model_path.empty()) {
                std::cerr << "LiveCapitalBootstrap prepare failed with VS_V2_MODEL_PATH set — fail-closed\n";
                return 4;
            }
            std::cerr << "LiveCapitalBootstrap prepare failed (model/weights) — continuing with defaults\n";
        }

        std::cout << "VS-V2 market-core LIVE chain starting epic=" << creds.epic
                  << " execution=bound open_positions="
                  << runtime.pipeline().open_positions().size() << std::endl;
        bootstrap.run(feed, g_running);
        std::cout << "VS-V2 market-core live path stopped" << std::endl;
        return 0;
    }

    // PAPER / REPLAY: no Capital LIVE gateway — fail-closed for broker health.
    runtime.pipeline().set_operating_mode(
        runtime_mode == mr::RuntimeMode::Replay ? mr::OperatingMode::Replay
                                                : mr::OperatingMode::Paper);
    runtime.pipeline().set_broker_healthy(false);
    std::cout << "VS-V2 market-core paper runtime persistent mode=" << mode << std::endl;
    runtime.run_paper(g_running);
    std::cout << "VS-V2 market-core paper runtime stopped" << std::endl;
    return 0;
}
