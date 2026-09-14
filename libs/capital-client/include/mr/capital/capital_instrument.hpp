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
private:
    std::unordered_map<InstrumentId, std::string> map_;
};
}