#include "mr/perception_engine/perception_engine.hpp"
namespace mr {
void PerceptionEngineFacade::on_event(const NormalizedEvent& e, double mid) {
    double price = mid;
    if (price <= 0) {
        if (e.last) price = *e.last;
        else if (e.bid && e.ask) price = (*e.bid + *e.ask) * 0.5;
    }
    if (price > 0) engine_.update(price, e.normalized_timestamp);
}
}