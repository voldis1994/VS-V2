#pragma once
#include "mr/market_data/subscription.hpp"
#include "mr/market_types/quote.hpp"
namespace mr {
class QuoteStream {
public:
    void on_quote(const Quote& q);
    void add_handler(QuoteHandler h);
private:
    std::vector<QuoteHandler> handlers_;
};
}