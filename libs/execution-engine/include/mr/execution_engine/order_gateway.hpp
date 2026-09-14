#pragma once
#include "mr/capital/capital_order.hpp"
#include <string>

namespace mr {

/**
 * Transport for Capital order lifecycle.
 * ExecutionEngine never decides BUY/SELL — it only submits approved intents.
 */
class OrderGateway {
public:
    virtual ~OrderGateway() = default;
    virtual CapitalOrderResponse create_position(const CapitalOrderRequest& request) = 0;
    virtual CapitalOrderResponse close_position(const std::string& deal_id) = 0;
    [[nodiscard]] virtual bool healthy() const = 0;
};

/** CapitalClient-backed gateway. */
class CapitalOrderGateway final : public OrderGateway {
public:
    explicit CapitalOrderGateway(class CapitalClient& client);
    CapitalOrderResponse create_position(const CapitalOrderRequest& request) override;
    CapitalOrderResponse close_position(const std::string& deal_id) override;
    [[nodiscard]] bool healthy() const override;

private:
    CapitalClient& client_;
};

}  // namespace mr
