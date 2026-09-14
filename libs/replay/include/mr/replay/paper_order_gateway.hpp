#pragma once

#include "mr/execution_engine/order_gateway.hpp"
#include <string>
#include <vector>

namespace mr {

/**
 * Deterministic paper fills for replay — never Capital LIVE transport.
 * Always healthy; fills at request price.
 */
class PaperOrderGateway final : public OrderGateway {
public:
    CapitalOrderResponse create_position(const CapitalOrderRequest& request) override;
    CapitalOrderResponse close_position(const std::string& deal_id) override;
    [[nodiscard]] bool healthy() const override { return true; }

    [[nodiscard]] const std::vector<CapitalOrderRequest>& creates() const { return creates_; }
    [[nodiscard]] const std::vector<std::string>& closes() const { return closes_; }
    void reset();

private:
    int seq_{0};
    std::vector<CapitalOrderRequest> creates_;
    std::vector<std::string> closes_;
};

}  // namespace mr
