#include "mr/execution_engine/order_gateway.hpp"
#include "mr/capital/capital_client.hpp"

namespace mr {

CapitalOrderGateway::CapitalOrderGateway(CapitalClient& client) : client_(client) {}

CapitalOrderResponse CapitalOrderGateway::create_position(const CapitalOrderRequest& request) {
    return client_.create_position(request);
}

CapitalOrderResponse CapitalOrderGateway::close_position(const std::string& deal_id) {
    return client_.close_position(deal_id);
}

bool CapitalOrderGateway::healthy() const {
    return client_.is_connected() && !client_.needs_reauth();
}

}  // namespace mr
