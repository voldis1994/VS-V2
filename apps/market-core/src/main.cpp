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

    if (runtime_mode == mr::RuntimeMode::Live || runtime_mode == mr::RuntimeMode::Demo || runtime_mode == mr::RuntimeMode::Shadow) {
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
                : (runtime_mode == mr::RuntimeMode::Demo
                       ? "https://demo-api-capital.backend-capital.com"
                       : "https://api-capital.backend-capital.com");

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
        if (runtime_mode == mr::RuntimeMode::Live) {
            boot.operating_mode = mr::OperatingMode::Live;
        } else if (runtime_mode == mr::RuntimeMode::Shadow) {
            boot.operating_mode = mr::OperatingMode::Shadow;
        } else {
            boot.operating_mode = mr::OperatingMode::Demo;
        }
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

    if (runtime_mode == mr::RuntimeMode::Replay) {
        runtime.pipeline().set_operating_mode(mr::OperatingMode::Replay);
        runtime.pipeline().set_broker_healthy(false);
        std::cout << "VS-V2 market-core REPLAY idle (no Capital execution)\n";
        runtime.run_paper(g_running);
        return 0;
    }

    // PAPER: Capital LIVE market data allowed; broker order gateway absolutely forbidden.
    runtime.pipeline().set_operating_mode(mr::OperatingMode::Paper);
    runtime.pipeline().set_broker_healthy(false);

    const auto paper_creds = mr::load_capital_credentials_from_env();
    if (paper_creds.complete()) {
        const std::string base_url =
            !paper_creds.base_url.empty()
                ? paper_creds.base_url
                : "https://api-capital.backend-capital.com";  // LIVE market data endpoint

        mr::CapitalClient client(base_url);
        client.connect();
        if (!client.authenticate(paper_creds.api_key, paper_creds.api_password,
                                 paper_creds.identifier)) {
            std::cerr << "Capital authenticate failed status=" << client.last_http_status()
                      << " — PAPER data path fail-closed\n";
            return 3;
        }
        client.instruments().set(1, paper_creds.epic);

        mr::LiveFeedConfig feed;
        feed.instrument = 1;
        feed.source = 1;
        feed.epic = paper_creds.epic;

        mr::LiveCapitalBootstrapConfig boot;
        boot.instrument = 1;
        boot.epic = paper_creds.epic;
        boot.operating_mode = mr::OperatingMode::Paper;
        boot.enable_execution = false;  // HARD: no real broker orders in PAPER
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
                std::cerr << "PAPER bootstrap prepare failed with VS_V2_MODEL_PATH — fail-closed\n";
                return 4;
            }
            std::cerr << "PAPER bootstrap prepare failed (model/weights) — continuing with defaults\n";
        }
        if (runtime.pipeline().has_execution()) {
            std::cerr << "FATAL: PAPER must not bind execution gateway\n";
            return 5;
        }

        std::cout << "VS-V2 market-core PAPER data-only epic=" << paper_creds.epic
                  << " execution=DISABLED open_positions="
                  << runtime.pipeline().open_positions().size() << std::endl;
        bootstrap.run(feed, g_running);
        std::cout << "VS-V2 market-core PAPER data path stopped" << std::endl;
        return 0;
    }

    std::cout << "VS-V2 market-core PAPER idle (no Capital credentials; no broker orders)\n";
    runtime.run_paper(g_running);
    std::cout << "VS-V2 market-core paper runtime stopped" << std::endl;
    return 0;
}
