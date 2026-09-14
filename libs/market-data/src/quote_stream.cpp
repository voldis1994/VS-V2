#include "mr/market_data/quote_stream.hpp"
namespace mr {
void QuoteStream::on_quote(const Quote& q) { for (auto& h : handlers_) if (h) h(q); }
void QuoteStream::add_handler(QuoteHandler h) { handlers_.push_back(std::move(h)); }
}