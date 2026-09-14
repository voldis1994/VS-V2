#include "mr/market_core/runtime.hpp"
#include "mr/brain/brain_version.hpp"
#include <iostream>
#include <string>

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
    runtime.configure(config, mr::parse_runtime_mode(mode));
    std::cout << "VS-V2 market-core ready mode=" << mode
              << " brain=" << mr::kBrainVersion << std::endl;
    return 0;
}
