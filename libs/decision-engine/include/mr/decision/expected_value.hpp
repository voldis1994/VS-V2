#pragma once
namespace mr {
inline double expected_value(double probability, double win, double loss, double cost) {
    return probability * win - (1.0 - probability) * loss - cost;
}
}
