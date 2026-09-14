#include "mr/brain_core/brain_state.hpp"
namespace mr {
void BrainState::update(const BrainContext& ctx) {
    snapshot_.instruments[ctx.instrument] = ctx;
    snapshot_.ts = ctx.ts;
}
}