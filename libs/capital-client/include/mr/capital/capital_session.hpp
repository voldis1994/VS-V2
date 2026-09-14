#pragma once
#include <string>
namespace mr {
struct CapitalSession { std::string cst; std::string security_token; bool active{false}; };
}