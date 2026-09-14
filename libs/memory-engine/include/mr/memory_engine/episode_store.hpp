#pragma once

#include "mr/memory_engine/episode_types.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace mr {

/** Restart-safe JSON episode persistence for Stage-8 learning datasets. */
class EpisodeStore {
public:
    explicit EpisodeStore(std::filesystem::path root);

    void save(const TradeEpisode& episode) const;
    [[nodiscard]] TradeEpisode load(const std::string& episode_id) const;
    [[nodiscard]] bool exists(const std::string& episode_id) const;
    [[nodiscard]] std::vector<std::string> list_ids() const;
    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    [[nodiscard]] std::filesystem::path path_for(const std::string& episode_id) const;
    std::filesystem::path root_;
};

}  // namespace mr
