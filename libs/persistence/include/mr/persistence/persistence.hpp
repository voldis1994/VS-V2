#pragma once
#include "mr/market_types/market_event.hpp"
#include <string>
namespace mr {
class IPersistence {
public:
    virtual ~IPersistence() = default;
    virtual void store(const MarketEvent& e) = 0;
    virtual void flush() = 0;
};
}