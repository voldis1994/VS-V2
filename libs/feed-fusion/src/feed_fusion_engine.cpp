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
        auto hit = it->second.health_by_source.find(sid);
        if (hit == it->second.health_by_source.end()) continue;
        auto h = hit->second;
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
}