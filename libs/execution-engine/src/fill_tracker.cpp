#include "mr/execution_engine/fill_tracker.hpp"
namespace mr {
void FillTracker::record(const FillRecord& f) { fills_.push_back(f); }
}