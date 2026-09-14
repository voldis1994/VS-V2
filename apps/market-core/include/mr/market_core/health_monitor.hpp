#pragma once

#include "mr/common/types.hpp"
#include "mr/telemetry/telemetry_hub.hpp"
#include <chrono>
#include <string>
#include <unordered_map>

namespace mr {

struct ComponentHealth {
    std::string name;
    HealthStatus status{HealthStatus::Healthy};
    std::chrono::steady_clock::time_point last_heartbeat{};
    std::string detail;
};

class HealthMonitor {
public:
    void heartbeat(const std::string& component, HealthStatus status, const std::string& detail = {});
    [[nodiscard]] HealthManager& manager() { return manager_; }
    [[nodiscard]] std::vector<ComponentHealth> snapshot() const;

private:
    HealthManager manager_;
    std::unordered_map<std::string, ComponentHealth> components_;
};

}  // namespace mr
