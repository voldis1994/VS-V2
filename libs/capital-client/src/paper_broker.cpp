#include "mr/capital/capital_client.hpp"
#include <unordered_map>
namespace mr {
class PaperCapitalClient {
public:
    bool connected{false};
    CapitalAccount account{"paper", 100000.0, 100000.0, 100000.0, "USD"};
    std::unordered_map<InstrumentId, CapitalQuote> quotes;
    std::vector<CapitalPosition> open;
    std::uint64_t deal_counter{1};
};
}
