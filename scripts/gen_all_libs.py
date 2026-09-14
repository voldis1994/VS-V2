#!/usr/bin/env python3
from pathlib import Path
import os, textwrap

ROOT = Path("/workspace")

def w(path, content):
    p = ROOT / path
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(textwrap.dedent(content).lstrip("\n") if content.startswith("\n") else content)

# Run common generator first
exec(open(ROOT / "scripts/generate_cpp_libs.py").read())

# ── market-types ────────────────────────────────────────────────────────────
MT_HEADERS = {
"instrument.hpp": """
#pragma once
#include "mr/common/id.hpp"
#include <string>
namespace mr {
struct Instrument {
    InstrumentId id{kInvalidInstrument};
    std::string symbol;
    std::string epic;
    double tick_size{0.01};
    double lot_size{1.0};
};
}""",
"price.hpp": """
#pragma once
#include <cmath>
namespace mr {
struct Price { double value{0}; [[nodiscard]] bool valid() const { return value > 0 && std::isfinite(value); } };
inline double mid(double bid, double ask) { return (bid + ask) * 0.5; }
}""",
"timestamp.hpp": """
#pragma once
#include "mr/common/id.hpp"
namespace mr {
inline std::int64_t to_ms(Timestamp ts) { return ts.count() / 1'000'000; }
inline Timestamp from_ms(std::int64_t ms) { return Timestamp(ms * 1'000'000LL); }
}""",
"timeframe.hpp": """
#pragma once
#include <cstdint>
namespace mr {
enum class Timeframe : std::uint8_t { Second1=1, Second10=10, Minute1=60, Minute5=300, Minute15=900, Hour1=3600 };
inline std::uint64_t timeframe_ns(Timeframe tf) {
    switch(tf) {
        case Timeframe::Second1: return 1'000'000'000ULL;
        case Timeframe::Second10: return 10'000'000'000ULL;
        case Timeframe::Minute1: return 60'000'000'000ULL;
        case Timeframe::Minute5: return 300'000'000'000ULL;
        case Timeframe::Minute15: return 900'000'000'000ULL;
        case Timeframe::Hour1: return 3'600'000'000'000ULL;
    }
    return 1'000'000'000ULL;
}
}""",
"spread.hpp": """
#pragma once
#include "mr/market_types/price.hpp"
namespace mr {
struct Spread { double bid{0}; double ask{0};
    [[nodiscard]] double width() const { return ask - bid; }
    [[nodiscard]] double mid_price() const { return mid(bid, ask); }
};
}""",
"quote.hpp": """
#pragma once
#include "mr/market_types/spread.hpp"
#include "mr/common/id.hpp"
namespace mr {
struct Quote {
    InstrumentId instrument{kInvalidInstrument};
    SourceId source{kInvalidSource};
    Spread spread{};
    double last{0};
    Timestamp exchange_ts{};
    Timestamp receive_ts{};
    bool valid{false};
};
}""",
"candle_status.hpp": """
#pragma once
namespace mr {
enum class CandleStatus : std::uint8_t { Forming=0, Closed=1, Gap=2, Invalid=3 };
}""",
"candle.hpp": """
#pragma once
#include "mr/market_types/candle_status.hpp"
#include "mr/common/id.hpp"
#include <cmath>
namespace mr {
struct Candle {
    InstrumentId instrument{kInvalidInstrument};
    Timestamp open_time{};
    double open{0}, high{0}, low{0}, close{0};
    std::uint32_t ticks{0};
    CandleStatus status{CandleStatus::Forming};
    [[nodiscard]] double body() const { return close - open; }
    [[nodiscard]] double range() const { return high - low; }
    [[nodiscard]] double body_pct() const {
        const double m = std::max(std::abs(open), 1e-9);
        return body() / m;
    }
};
}""",
"data_source.hpp": """
#pragma once
#include "mr/common/id.hpp"
#include <string>
namespace mr {
struct DataSource { SourceId id{kInvalidSource}; std::string name; HealthStatus health{HealthStatus::Disconnected}; };
}""",
"data_quality.hpp": """
#pragma once
#include <cstdint>
namespace mr {
enum class DataQualityFlag : std::uint32_t {
    None=0, Stale=1<<0, OutOfOrder=1<<1, Duplicate=1<<2, SequenceGap=1<<3,
    Crossed=1<<4, WideSpread=1<<5, MissingField=1<<6, Divergent=1<<7
};
using DataQualityFlags = std::uint32_t;
inline DataQualityFlags operator|(DataQualityFlag a, DataQualityFlag b) {
    return static_cast<DataQualityFlags>(a)|static_cast<DataQualityFlags>(b);
}
inline bool has_flag(DataQualityFlags f, DataQualityFlag flag) {
    return (f & static_cast<DataQualityFlags>(flag)) != 0;
}
}""",
"market_event.hpp": """
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
}""",
}

for name, body in MT_HEADERS.items():
    w(f"libs/market-types/include/mr/market_types/{name}", body)

w("libs/market-types/CMakeLists.txt", """
add_library(mr_market_types INTERFACE)
add_library(mr::market-types ALIAS mr_market_types)
target_include_directories(mr_market_types INTERFACE include)
target_link_libraries(mr_market_types INTERFACE mr::common)
target_compile_features(mr_market_types INTERFACE cxx_std_20)
""")
print("market-types done")
