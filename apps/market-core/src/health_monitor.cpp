#include "mr/market_core/health_monitor.hpp"

namespace mr {

void HealthMonitor::heartbeat(const std::string& component, HealthStatus status, const std::string& detail) {
    ComponentHealth h;
    h.name = component;
    h.status = status;
    h.detail = detail;
    h.last_heartbeat = std::chrono::steady_clock::now();
    components_[component] = h;
    manager_.set_component(component, status);
}

std::vector<ComponentHealth> HealthMonitor::snapshot() const {
    std::vector<ComponentHealth> out;
    for (const auto& [_, h] : components_) out.push_back(h);
    return out;
}

}  // namespace mr
