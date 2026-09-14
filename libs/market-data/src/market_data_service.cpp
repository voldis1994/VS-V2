#include "mr/market_data/market_data_service.hpp"
namespace mr {
void MarketDataService::add_provider(std::shared_ptr<IMarketDataProvider> p) { providers_.push_back(std::move(p)); }
void MarketDataService::start() {
    for (auto& p : providers_) {
        p->start([this](const MarketEvent& e) {
            if (!e.bid || !e.ask) return;
            Quote q; q.instrument = e.instrument; q.source = e.source;
            q.spread.bid = *e.bid; q.spread.ask = *e.ask;
            q.last = e.last ? *e.last : q.spread.mid_price();
            q.exchange_ts = e.exchange_timestamp; q.receive_ts = e.receive_timestamp;
            q.valid = true; stream_.on_quote(q); subs_.dispatch(q);
        });
    }
}
void MarketDataService::stop() { for (auto& p : providers_) p->stop(); }
}