#include "mr/execution_engine/order_builder.hpp"
namespace mr {
CapitalOrderRequest OrderBuilder::from_intent(const TradeIntent& intent, double quantity) const {
    CapitalOrderRequest r; r.instrument = intent.instrument; r.direction = intent.direction;
    r.quantity = quantity; r.price = intent.reference_price;
    r.stop_loss = intent.stop_loss; r.take_profit = intent.take_profit; return r;
}
}