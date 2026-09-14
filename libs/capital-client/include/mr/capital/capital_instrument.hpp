#pragma once
#include "mr/common/id.hpp"
#include <optional>
#include <string>
#include <unordered_map>
namespace mr {
class CapitalInstrumentMap {
public:
    void set(InstrumentId id, const std::string& epic);
    [[nodiscard]] std::optional<std::string> epic(InstrumentId id) const;
    [[nodiscard]] std::optional<InstrumentId> find_by_epic(const std::string& epic) const;
private:
    std::unordered_map<InstrumentId, std::string> map_;
};
}
