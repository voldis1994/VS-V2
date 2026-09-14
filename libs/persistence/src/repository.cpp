#include "mr/persistence/repository.hpp"
#include "mr/persistence/raw_event_storage.hpp"
namespace mr {
EventRepository::EventRepository(const std::string& path) : path_(path) {}
void EventRepository::store(const MarketEvent& e) { RawEventWriter w(path_); w.write(e); count_++; }
void EventRepository::flush() {}
}