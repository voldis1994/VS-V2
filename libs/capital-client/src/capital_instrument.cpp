#include "mr/capital/capital_instrument.hpp"
namespace mr {
void CapitalInstrumentMap::set(InstrumentId id, const std::string& epic) { map_[id] = epic; }
std::optional<std::string> CapitalInstrumentMap::epic(InstrumentId id) const {
    auto it = map_.find(id); return it == map_.end() ? std::nullopt : std::optional<std::string>(it->second);
}
}