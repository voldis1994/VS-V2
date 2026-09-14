#pragma once
#include "mr/capital/capital_client.hpp"
#include "mr/execution_engine/order_builder.hpp"
namespace mr {
class CapitalExecutor {
public:
    explicit CapitalExecutor(CapitalClient& client) : client_(client) {}
    CapitalOrderResponse execute(const CapitalOrderRequest& req);
    CapitalOrderResponse close(const std::string& deal_id);
private:
    CapitalClient& client_;
};
}