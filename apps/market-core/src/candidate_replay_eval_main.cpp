#include "mr/market_core/candidate_replay_eval.hpp"
#include "mr/market_core/weight_bundle.hpp"
#include "mr/memory_engine/episode_store.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using json = nlohmann::json;
using namespace mr;

static void usage() {
    std::cerr << "Usage: candidate-replay-eval --episodes-dir DIR --weights FILE [--equity N]\n";
}

int main(int argc, char** argv) {
    std::filesystem::path episodes_dir;
    std::filesystem::path weights_path;
    double equity = 50'000.0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--episodes-dir" && i + 1 < argc) {
            episodes_dir = argv[++i];
        } else if (a == "--weights" && i + 1 < argc) {
            weights_path = argv[++i];
        } else if (a == "--equity" && i + 1 < argc) {
            equity = std::stod(argv[++i]);
        } else if (a == "--help") {
            usage();
            return 0;
        } else {
            usage();
            return 2;
        }
    }
    if (episodes_dir.empty() || weights_path.empty()) {
        usage();
        return 2;
    }

    std::ifstream win(weights_path);
    if (!win) {
        std::cerr << "failed to read weights: " << weights_path << "\n";
        return 1;
    }
    json wj;
    win >> wj;
    const WeightBundle weights =
        WeightBundle::from_json(wj.contains("weights") ? wj.at("weights") : wj);

    EpisodeStore store(episodes_dir);
    std::vector<TradeEpisode> episodes;
    for (const auto& id : store.list_ids()) {
        auto ep = store.load(id);
        if (ep.sealed && ep.outcome.sealed) episodes.push_back(std::move(ep));
    }

    const auto metrics = evaluate_candidate_replay(episodes, weights, equity);
    json out{
        {"edge", metrics.edge},
        {"mean_pnl", metrics.mean_pnl},
        {"win_rate", metrics.win_rate},
        {"n", metrics.n},
        {"market_events", metrics.market_events},
        {"used_production_replay", metrics.used_production_replay},
        {"source", "stage7_episode_replay"},
    };
    std::cout << out.dump(2) << "\n";
    return 0;
}
