#pragma once
#include <cmath>
namespace mr {
struct Price { double value{0}; [[nodiscard]] bool valid() const { return value > 0 && std::isfinite(value); } };
inline double mid(double bid, double ask) { return (bid + ask) * 0.5; }
}