#pragma once
#include "mr/pattern_engine/pattern_vector.hpp"
namespace mr {
class PatternSimilarity {
public:
    [[nodiscard]] double cosine(const PatternVector& a, const PatternVector& b) const;
};
}