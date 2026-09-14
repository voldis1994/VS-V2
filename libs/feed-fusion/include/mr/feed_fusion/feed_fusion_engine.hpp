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
}