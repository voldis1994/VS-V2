#pragma once
#include "mr/common/id.hpp"
#include <string>
namespace mr {
struct DataSource { SourceId id{kInvalidSource}; std::string name; HealthStatus health{HealthStatus::Disconnected}; };
}