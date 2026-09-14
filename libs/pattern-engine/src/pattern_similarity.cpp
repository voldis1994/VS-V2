#include "mr/pattern_engine/pattern_similarity.hpp"
#include <cmath>
#include <numeric>
namespace mr {
double PatternSimilarity::cosine(const PatternVector& a, const PatternVector& b) const {
    if (a.size() != b.size() || a.empty()) return 0;
    double dot=0, na=0, nb=0;
    for (std::size_t i=0;i<a.size();++i) { dot += a[i]*b[i]; na += a[i]*a[i]; nb += b[i]*b[i]; }
    if (na <= 0 || nb <= 0) return 0;
    return dot / (std::sqrt(na)*std::sqrt(nb));
}
}