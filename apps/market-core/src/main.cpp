#include "mr/market_core/pipeline.hpp"
#include <iostream>
int main() {
    mr::MarketCorePipeline pipeline;
    std::cout << "market-core ready (" << mr::kBrainVersion << ")" << std::endl;
    return 0;
}