#include "mr/memory_engine/memory_engine.hpp"
namespace mr {
void MemoryEngine::update(double price, const PriceDynamics& pd, bool trade_closed, double pnl) {
    bank_.working.recent_returns.push_back(pd.normalized_return);
    if (bank_.working.recent_returns.size() > bank_.working.max_size) bank_.working.recent_returns.pop_front();
    bank_.session.event_count++;
    if (bank_.session.session_high == 0 || price > bank_.session.session_high) bank_.session.session_high = price;
    if (bank_.session.session_low == 0 || price < bank_.session.session_low) bank_.session.session_low = price;
    bank_.historical.closes.push_back(price);
    if (bank_.historical.closes.size() > 500) bank_.historical.closes.pop_front();
    if (pd.velocity > 0 && price > bank_.structural.last_swing_high) bank_.structural.last_swing_high = price;
    if (pd.velocity < 0 && (bank_.structural.last_swing_low == 0 || price < bank_.structural.last_swing_low))
        bank_.structural.last_swing_low = price;
    if (trade_closed) {
        if (pnl >= 0) { bank_.trades.wins++; bank_.trades.avg_win = (bank_.trades.avg_win * (bank_.trades.wins-1) + pnl) / bank_.trades.wins; }
        else { bank_.trades.losses++; bank_.trades.avg_loss = (bank_.trades.avg_loss * (bank_.trades.losses-1) + std::abs(pnl)) / bank_.trades.losses; }
    }
}
}