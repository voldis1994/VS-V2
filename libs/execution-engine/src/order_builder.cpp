#include "mr/execution_engine/order_builder.hpp"
#include <cstdint>
#include <string>
namespace mr {
CapitalOrderRequest OrderBuilder::from_intent(const TradeIntent& intent, double quantity) const {
    CapitalOrderRequest r;
    r.instrument = intent.instrument;
    r.direction = intent.direction;
    r.quantity = quantity;
    r.price = intent.reference_price;
    r.stop_loss = intent.stop_loss;
    r.take_profit = intent.take_profit;
    // Stable client key for broker-side idempotency (LIVE hardening).
    r.client_order_id = "vs2-" + std::to_string(static_cast<std::uint64_t>(intent.id));
    return r;
}
}
