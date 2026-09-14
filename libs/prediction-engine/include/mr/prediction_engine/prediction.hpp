#pragma once
namespace mr {
struct Prediction {
    double probability{0.5}, uncertainty{0.5};
    double expected_mfe{0}, expected_mae{0}, expected_duration_s{0};
};
}