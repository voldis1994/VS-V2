#pragma once
#include <string>

namespace mr {

struct CapitalAccount {
    std::string account_id;
    double balance{0};
    double available{0};
    /** Prefer nested balance.balance / equity; used by RiskEngine. */
    double equity{0};
    std::string currency{"USD"};
};

}  // namespace mr
