#pragma once
#include "mr/persistence/persistence.hpp"
#include <vector>
namespace mr {
class Transaction {
public:
    explicit Transaction(IPersistence& backend) : backend_(backend) {}
    void append(const MarketEvent& e) { batch_.push_back(e); }
    void commit();
    void rollback() { batch_.clear(); }
private:
    IPersistence& backend_;
    std::vector<MarketEvent> batch_;
};
}