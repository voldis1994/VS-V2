#pragma once
#include "mr/common/id.hpp"
#include <functional>
namespace mr {
using QuoteHandler = std::function<void(const Quote&)>;
struct Subscription {
    InstrumentId instrument{kInvalidInstrument};
    SourceId source{kInvalidSource};
    QuoteHandler handler;
};
class SubscriptionManager {
public:
    void subscribe(const Subscription& sub);
    void unsubscribe(InstrumentId instrument, SourceId source);
    void dispatch(const Quote& q);
private:
    std::vector<Subscription> subs_;
};
}