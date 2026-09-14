#pragma once
namespace mr {
struct MicrostructureFeatures {
    double spread{0}, microprice{0}, bid_ask_imbalance{0};
    double aggressive_buy_pressure{0}, aggressive_sell_pressure{0};
    double exhaustion_proxy{0}, rejection_proxy{0}, reclaim_proxy{0};
};
}