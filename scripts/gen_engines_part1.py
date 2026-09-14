#!/usr/bin/env python3
from pathlib import Path
import textwrap
ROOT = Path("/workspace")
def w(p, c):
    path = ROOT / p
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(textwrap.dedent(c).lstrip("\n"))

# feed-fusion
w("libs/feed-fusion/include/mr/feed_fusion/feed_state.hpp", """
#pragma once
#include "mr/market_types/market_event.hpp"
#include "mr/data_quality/quality_state.hpp"
#include "mr/common/ring_buffer.hpp"
#include <unordered_map>
namespace mr {
struct ReactionEvent { SourceId source{kInvalidSource}; Timestamp timestamp{}; double price_change{0}; };
struct InstrumentFeedState {
    std::unordered_map<SourceId, NormalizedEvent> last_by_source;
    std::unordered_map<SourceId, SourceHealth> health_by_source;
    RingBuffer<ReactionEvent, 64> reactions;
    double last_mid{0};
};
}""")

w("libs/feed-fusion/include/mr/feed_fusion/consensus_quote.hpp", """
#pragma once
#include "mr/common/id.hpp"
namespace mr {
struct ConsensusQuote {
    double mid{0}, spread{0}, confidence{0};
    std::uint32_t sources{0};
    [[nodiscard]] bool valid() const { return mid > 0 && sources > 0; }
};
}""")

w("libs/feed-fusion/include/mr/feed_fusion/feed_fusion_engine.hpp", """
#pragma once
#include "mr/feed_fusion/feed_state.hpp"
#include "mr/feed_fusion/consensus_quote.hpp"
namespace mr {
struct FeedDivergence { double max{0}, mean{0}; SourceId worst{kInvalidSource}; };
struct LeadLagState { SourceId leader{kInvalidSource}; double probability{0}; double agreement{0}; };
class FeedFusionEngine {
public:
    void ingest(const NormalizedEvent& event, const SourceHealth& health);
    [[nodiscard]] ConsensusQuote consensus(InstrumentId instrument) const;
    [[nodiscard]] FeedDivergence divergence(InstrumentId instrument) const;
    [[nodiscard]] LeadLagState lead_lag(InstrumentId instrument) const;
    [[nodiscard]] std::vector<SourceWeight> weights(InstrumentId instrument) const;
    [[nodiscard]] NormalizedEvent fused_event(InstrumentId instrument) const;
private:
    std::unordered_map<InstrumentId, InstrumentFeedState> state_;
    [[nodiscard]] double mid(const NormalizedEvent& e) const;
    void track_reaction(InstrumentId inst, const NormalizedEvent& e, double change);
};
}""")

w("libs/feed-fusion/src/feed_fusion_engine.cpp", """
#include "mr/feed_fusion/feed_fusion_engine.hpp"
#include "mr/common/clock.hpp"
#include <cmath>
#include <numeric>
namespace mr {
double FeedFusionEngine::mid(const NormalizedEvent& e) const {
    if (e.bid && e.ask) return (*e.bid + *e.ask) * 0.5;
    if (e.last) return *e.last;
    return 0;
}
void FeedFusionEngine::track_reaction(InstrumentId inst, const NormalizedEvent& e, double change) {
    if (std::abs(change) <= 1e-10) return;
    ReactionEvent r{e.source, e.normalized_timestamp, change};
    state_[inst].reactions.push(r);
}
void FeedFusionEngine::ingest(const NormalizedEvent& event, const SourceHealth& health) {
    auto& st = state_[event.instrument];
    double m = mid(event);
    double prev = st.last_mid;
    st.last_by_source[event.source] = event;
    st.health_by_source[event.source] = health;
    if (m > 0) { track_reaction(event.instrument, event, prev > 0 ? m - prev : 0); st.last_mid = m; }
}
ConsensusQuote FeedFusionEngine::consensus(InstrumentId instrument) const {
    ConsensusQuote c;
    auto it = state_.find(instrument); if (it == state_.end()) return c;
    double wsum = 0, weights = 0, spread_sum = 0; std::uint32_t n = 0;
    for (const auto& [sid, ev] : it->second.last_by_source) {
        auto h = it->second.health_by_source[sid];
        double w = h.reliability * (h.predictive_usefulness + 0.5);
        double m = mid(ev); if (m <= 0 || w <= 0) continue;
        wsum += m * w; weights += w;
        if (ev.bid && ev.ask) spread_sum += *ev.ask - *ev.bid;
        ++n;
    }
    if (weights > 0) {
        c.mid = wsum / weights; c.spread = n ? spread_sum / n : 0;
        c.confidence = std::min(1.0, weights / n); c.sources = n;
    }
    return c;
}
FeedDivergence FeedFusionEngine::divergence(InstrumentId instrument) const {
    FeedDivergence d; auto c = consensus(instrument); if (c.mid <= 0) return d;
    auto it = state_.find(instrument); if (it == state_.end()) return d;
    std::vector<double> divs;
    for (const auto& [sid, ev] : it->second.last_by_source) {
        double m = mid(ev); if (m <= 0) continue;
        double dv = std::abs(m - c.mid); divs.push_back(dv);
        if (dv > d.max) { d.max = dv; d.worst = sid; }
    }
    if (!divs.empty()) d.mean = std::accumulate(divs.begin(), divs.end(), 0.0) / divs.size();
    return d;
}
LeadLagState FeedFusionEngine::lead_lag(InstrumentId instrument) const {
    LeadLagState ll; auto it = state_.find(instrument);
    if (it == state_.end() || it->second.reactions.size() < 2) return ll;
    std::unordered_map<SourceId, std::uint64_t> leads; std::uint64_t total = 0;
    for (std::size_t i = 1; i < it->second.reactions.size(); ++i) {
        if (std::abs(it->second.reactions.at(i).price_change) > 1e-10) {
            leads[it->second.reactions.at(i).source]++; total++;
        }
    }
    for (const auto& [s, c] : leads) if (c > ll.probability * total) { ll.leader = s; ll.probability = total ? c / static_cast<double>(total) : 0; }
    ll.agreement = ll.probability; return ll;
}
std::vector<SourceWeight> FeedFusionEngine::weights(InstrumentId instrument) const {
    std::vector<SourceWeight> out; auto it = state_.find(instrument); if (it == state_.end()) return out;
    double total = 0;
    for (const auto& [sid, h] : it->second.health_by_source) total += h.reliability;
    for (const auto& [sid, h] : it->second.health_by_source) {
        out.push_back({sid, total > 0 ? h.reliability / total : 0});
    }
    return out;
}
NormalizedEvent FeedFusionEngine::fused_event(InstrumentId instrument) const {
    NormalizedEvent f; f.instrument = instrument; auto c = consensus(instrument);
    if (c.valid()) { f.bid = c.mid - c.spread * 0.5; f.ask = c.mid + c.spread * 0.5; f.last = c.mid; }
    f.normalized_timestamp = now_utc_ns(); f.receive_timestamp = f.normalized_timestamp;
    f.type = MarketEventType::Quote; return f;
}
}""")

w("libs/feed-fusion/CMakeLists.txt", """
add_library(mr_feed_fusion STATIC src/feed_fusion_engine.cpp)
add_library(mr::feed-fusion ALIAS mr_feed_fusion)
target_include_directories(mr_feed_fusion PUBLIC include)
target_link_libraries(mr_feed_fusion PUBLIC mr::common mr::market-types mr::data-quality)
target_compile_features(mr_feed_fusion PUBLIC cxx_std_20)
""")

# market-data
w("libs/market-data/include/mr/market_data/subscription.hpp", """
#pragma once
#include "mr/common/id.hpp"
#include <functional>
namespace mr {
using QuoteHandler = std::function<void(const Quote&)>;
struct Subscription {
    InstrumentId instrument{kInvalidInstrument};
    SourceId source{kInvalidSource};
    QuoteHandler handler;
};
class SubscriptionManager {
public:
    void subscribe(const Subscription& sub);
    void unsubscribe(InstrumentId instrument, SourceId source);
    void dispatch(const Quote& q);
private:
    std::vector<Subscription> subs_;
};
}""")

w("libs/market-data/include/mr/market_data/ohlc_source.hpp", """
#pragma once
#include "mr/market_types/candle.hpp"
#include <vector>
namespace mr {
class OhlcSource {
public:
    void push(const Candle& c);
    [[nodiscard]] const std::vector<Candle>& history() const { return candles_; }
private:
    std::vector<Candle> candles_;
};
}""")

w("libs/market-data/include/mr/market_data/quote_stream.hpp", """
#pragma once
#include "mr/market_data/subscription.hpp"
#include "mr/market_types/quote.hpp"
namespace mr {
class QuoteStream {
public:
    void on_quote(const Quote& q);
    void add_handler(QuoteHandler h);
private:
    std::vector<QuoteHandler> handlers_;
};
}""")

w("libs/market-data/include/mr/market_data/market_data_service.hpp", """
#pragma once
#include "mr/market_data/quote_stream.hpp"
#include "mr/market_data/subscription.hpp"
#include "mr/market_types/market_event.hpp"
#include <functional>
namespace mr {
using MarketEventCallback = std::function<void(const MarketEvent&)>;
class IMarketDataProvider {
public:
    virtual ~IMarketDataProvider() = default;
    virtual SourceId source_id() const = 0;
    virtual void start(MarketEventCallback cb) = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool is_connected() const = 0;
    [[nodiscard]] virtual HealthStatus health() const = 0;
};
class MarketDataService {
public:
    void add_provider(std::shared_ptr<IMarketDataProvider> p);
    void start();
    void stop();
    QuoteStream& quotes() { return stream_; }
private:
    std::vector<std::shared_ptr<IMarketDataProvider>> providers_;
    QuoteStream stream_;
    SubscriptionManager subs_;
};
}""")

w("libs/market-data/src/subscription.cpp", """
#include "mr/market_data/subscription.hpp"
namespace mr {
void SubscriptionManager::subscribe(const Subscription& sub) { subs_.push_back(sub); }
void SubscriptionManager::unsubscribe(InstrumentId instrument, SourceId source) {
    subs_.erase(std::remove_if(subs_.begin(), subs_.end(), [&](const Subscription& s) {
        return s.instrument == instrument && s.source == source;
    }), subs_.end());
}
void SubscriptionManager::dispatch(const Quote& q) {
    for (const auto& s : subs_) if (s.instrument == q.instrument && s.handler) s.handler(q);
}
}""")

w("libs/market-data/src/quote_stream.cpp", """
#include "mr/market_data/quote_stream.hpp"
namespace mr {
void QuoteStream::on_quote(const Quote& q) { for (auto& h : handlers_) if (h) h(q); }
void QuoteStream::add_handler(QuoteHandler h) { handlers_.push_back(std::move(h)); }
}""")

w("libs/market-data/src/market_data_service.cpp", """
#include "mr/market_data/market_data_service.hpp"
namespace mr {
void MarketDataService::add_provider(std::shared_ptr<IMarketDataProvider> p) { providers_.push_back(std::move(p)); }
void MarketDataService::start() {
    for (auto& p : providers_) {
        p->start([this](const MarketEvent& e) {
            if (!e.bid || !e.ask) return;
            Quote q; q.instrument = e.instrument; q.source = e.source;
            q.spread.bid = *e.bid; q.spread.ask = *e.ask;
            q.last = e.last ? *e.last : q.spread.mid_price();
            q.exchange_ts = e.exchange_timestamp; q.receive_ts = e.receive_timestamp;
            q.valid = true; stream_.on_quote(q); subs_.dispatch(q);
        });
    }
}
void MarketDataService::stop() { for (auto& p : providers_) p->stop(); }
}""")

w("libs/market-data/src/ohlc_source.cpp", """
#include "mr/market_data/ohlc_source.hpp"
namespace mr {
void OhlcSource::push(const Candle& c) { candles_.push_back(c); }
}""")

w("libs/market-data/src/replay_provider.cpp", """
#include "mr/market_data/market_data_service.hpp"
#include <vector>
namespace mr {
class ReplayProvider : public IMarketDataProvider {
public:
    ReplayProvider(SourceId id, std::vector<MarketEvent> events) : id_(id), events_(std::move(events)) {}
    SourceId source_id() const override { return id_; }
    void start(MarketEventCallback cb) override { running_ = true; cb_ = std::move(cb); for (auto& e : events_) if (cb_) cb_(e); }
    void stop() override { running_ = false; }
    bool is_connected() const override { return running_; }
    HealthStatus health() const override { return running_ ? HealthStatus::Healthy : HealthStatus::Disconnected; }
private:
    SourceId id_; std::vector<MarketEvent> events_; MarketEventCallback cb_; bool running_{false};
};
}""")

w("libs/market-data/CMakeLists.txt", """
add_library(mr_market_data STATIC
    src/market_data_service.cpp src/quote_stream.cpp src/subscription.cpp src/ohlc_source.cpp)
add_library(mr::market-data ALIAS mr_market_data)
target_include_directories(mr_market_data PUBLIC include)
target_link_libraries(mr_market_data PUBLIC mr::common mr::market-types)
target_compile_features(mr_market_data PUBLIC cxx_std_20)
""")

print("feed-fusion + market-data done")
