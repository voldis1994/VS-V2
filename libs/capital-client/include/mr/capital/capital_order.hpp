#pragma once
#include "mr/common/id.hpp"
#include <string>
namespace mr {
struct CapitalOrderRequest {
    InstrumentId instrument{kInvalidInstrument};
    Direction direction{Direction::Flat};
    double quantity{0}, price{0}, stop_loss{0}, take_profit{0};
    std::string client_order_id;  // idempotency key for LIVE submits
};
struct CapitalOrderResponse {
    bool success{false}; std::string deal_id; double fill_price{0}, filled_quantity{0};
    std::string error_message;
};
struct CapitalPosition {
    std::string deal_id;
    std::string epic;
    InstrumentId instrument{kInvalidInstrument};
    Direction direction{Direction::Flat};
    double quantity{0}, entry_price{0}, unrealized_pnl{0};
};
}