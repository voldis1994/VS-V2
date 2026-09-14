#include "mr/microstructure_engine/microstructure_engine.hpp"
namespace mr {
void MicrostructureEngine::update(const NormalizedEvent& e) {
    if (e.bid && e.ask) {
        f_.spread = *e.ask - *e.bid;
        double bs = e.bid_size.value_or(1), as = e.ask_size.value_or(1);
        f_.microprice = (*e.bid * as + *e.ask * bs) / (bs + as);
        f_.bid_ask_imbalance = (bs - as) / (bs + as);
        if (e.last) {
            double mid = (*e.bid + *e.ask) * 0.5;
            f_.rejection_proxy = *e.last < mid ? f_.rejection_proxy * 0.8 + 0.2 : f_.rejection_proxy * 0.8;
            f_.reclaim_proxy = *e.last > mid ? f_.reclaim_proxy * 0.8 + 0.2 : f_.reclaim_proxy * 0.8;
        }
    }
    if (e.type == MarketEventType::Trade) {
        trade_count_++;
        if (e.trade_size && e.last && e.bid && e.ask) {
            double mid = (*e.bid + *e.ask) * 0.5;
            if (*e.last >= mid) f_.aggressive_buy_pressure += *e.trade_size;
            else f_.aggressive_sell_pressure += *e.trade_size;
        }
    } else quote_count_++;
    auto total = trade_count_ + quote_count_;
    if (total > 100 && f_.aggressive_buy_pressure + f_.aggressive_sell_pressure > 0) {
        double ratio = f_.aggressive_buy_pressure / (f_.aggressive_buy_pressure + f_.aggressive_sell_pressure);
        f_.exhaustion_proxy = ratio > 0.8 || ratio < 0.2 ? 0.7 : 0;
    }
}
}