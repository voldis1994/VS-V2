#include "mr/pattern_engine/pattern_encoder.hpp"
namespace mr {
PatternVector PatternEncoder::encode(const PriceDynamics& pd, const StructureFeatures& st) const {
    return {pd.normalized_return, pd.velocity, pd.acceleration, pd.directional_persistence,
            st.range_position, st.continuation_pressure, st.pullback_depth, st.breakout_strength};
}
}