#pragma once
#include "mr/market_types/data_quality.hpp"
#include "mr/common/id.hpp"
#include <optional>
namespace mr {
enum class MarketEventType : std::uint8_t { Unknown=0, Quote=1, Trade=2, BookUpdate=3, Heartbeat=4, SessionStatus=5 };
struct MarketEvent {
    InstrumentId instrument{kInvalidInstrument};
    SourceId source{kInvalidSource};
    Timestamp exchange_timestamp{};
    Timestamp provider_timestamp{};
    Timestamp receive_timestamp{};
    std::optional<double> bid, ask, last, bid_size, ask_size, trade_size;
    MarketEventType type{MarketEventType::Unknown};
    SequenceNumber sequence{0};
    DataQualityFlags quality{0};
};
struct NormalizedEvent : MarketEvent {
    Timestamp normalized_timestamp{};
    Timestamp processing_start{};
    Timestamp processing_end{};
};
}