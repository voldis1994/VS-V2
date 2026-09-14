#include "mr/replay/paper_order_gateway.hpp"

namespace mr {

CapitalOrderResponse PaperOrderGateway::create_position(const CapitalOrderRequest& request) {
    creates_.push_back(request);
    CapitalOrderResponse r;
    r.success = true;
    r.deal_id = "PAPER-" + std::to_string(++seq_);
    r.fill_price = request.price;
    r.filled_quantity = request.quantity;
    return r;
}

CapitalOrderResponse PaperOrderGateway::close_position(const std::string& deal_id) {
    closes_.push_back(deal_id);
    CapitalOrderResponse r;
    r.success = true;
    r.deal_id = deal_id;
    return r;
}

void PaperOrderGateway::reset() {
    creates_.clear();
    closes_.clear();
    seq_ = 0;
}

}  // namespace mr
