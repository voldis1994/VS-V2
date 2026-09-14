#pragma once
#include <string>
namespace mr {
struct CapitalAccount { std::string account_id; double balance{0}, available{0}; std::string currency; };
}