#include "mr/execution_engine/capital_executor.hpp"
namespace mr {
CapitalOrderResponse CapitalExecutor::execute(const CapitalOrderRequest& req) { return client_.create_position(req); }
CapitalOrderResponse CapitalExecutor::close(const std::string& deal_id) { return client_.close_position(deal_id); }
}