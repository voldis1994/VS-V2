#include "mr/capital/capital_instrument.hpp"
namespace mr {
void CapitalInstrumentMap::set(InstrumentId id, const std::string& epic) { map_[id] = epic; }
std::optional<std::string> CapitalInstrumentMap::epic(InstrumentId id) const {
    auto it = map_.find(id);
    return it == map_.end() ? std::nullopt : std::optional<std::string>(it->second);
}
std::optional<InstrumentId> CapitalInstrumentMap::find_by_epic(const std::string& epic) const {
    for (const auto& [id, e] : map_) {
        if (e == epic) return id;
    }
    return std::nullopt;
}
}
