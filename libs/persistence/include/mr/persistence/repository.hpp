#pragma once
#include "mr/persistence/persistence.hpp"
#include <vector>
namespace mr {
class EventRepository : public IPersistence {
public:
    explicit EventRepository(const std::string& path);
    void store(const MarketEvent& e) override;
    void flush() override;
    [[nodiscard]] std::uint64_t count() const { return count_; }
private:
    std::string path_;
    std::uint64_t count_{0};
};
}