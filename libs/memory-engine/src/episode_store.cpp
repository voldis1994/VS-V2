#include "mr/memory_engine/episode_store.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace mr {
namespace {

using json = nlohmann::json;

std::int64_t ts_count(Timestamp ts) { return ts.count(); }
Timestamp ts_from(std::int64_t c) { return Timestamp{c}; }

json side_to_json(const SidePrediction& s) {
    return json{
        {"direction", static_cast<int>(s.direction)},
        {"continuation", s.continuation},
        {"reversal_failure", s.reversal_failure},
        {"expected_move", s.expected_move},
        {"adverse_move", s.adverse_move},
        {"probability", s.probability},
        {"confidence", s.confidence},
        {"expected_value", s.expected_value},
        {"invalidation", s.invalidation},
        {"thesis_quality", s.thesis_quality},
        {"uncertainty", s.uncertainty},
    };
}

SidePrediction side_from_json(const json& j) {
    SidePrediction s;
    s.direction = static_cast<Direction>(j.value("direction", 0));
    s.continuation = j.value("continuation", 0.0);
    s.reversal_failure = j.value("reversal_failure", 0.0);
    s.expected_move = j.value("expected_move", 0.0);
    s.adverse_move = j.value("adverse_move", 0.0);
    s.probability = j.value("probability", 0.0);
    s.confidence = j.value("confidence", 0.0);
    s.expected_value = j.value("expected_value", 0.0);
    s.invalidation = j.value("invalidation", 0.0);
    s.thesis_quality = j.value("thesis_quality", 0.0);
    s.uncertainty = j.value("uncertainty", 0.0);
    return s;
}

json quote_to_json(const MarketEvent& e) {
    json j{
        {"instrument", e.instrument},
        {"source", e.source},
        {"exchange_timestamp", ts_count(e.exchange_timestamp)},
        {"provider_timestamp", ts_count(e.provider_timestamp)},
        {"receive_timestamp", ts_count(e.receive_timestamp)},
        {"type", static_cast<int>(e.type)},
        {"sequence", e.sequence},
        {"quality", e.quality},
    };
    if (e.bid) j["bid"] = *e.bid;
    if (e.ask) j["ask"] = *e.ask;
    if (e.last) j["last"] = *e.last;
    if (e.bid_size) j["bid_size"] = *e.bid_size;
    if (e.ask_size) j["ask_size"] = *e.ask_size;
    if (e.trade_size) j["trade_size"] = *e.trade_size;
    return j;
}

MarketEvent quote_from_json(const json& j) {
    MarketEvent e;
    e.instrument = j.value("instrument", 0u);
    e.source = j.value("source", 0u);
    e.exchange_timestamp = ts_from(j.value("exchange_timestamp", 0ll));
    e.provider_timestamp = ts_from(j.value("provider_timestamp", 0ll));
    e.receive_timestamp = ts_from(j.value("receive_timestamp", 0ll));
    e.type = static_cast<MarketEventType>(j.value("type", 0));
    e.sequence = j.value("sequence", 0ull);
    e.quality = j.value("quality", 0u);
    if (j.contains("bid")) e.bid = j.at("bid").get<double>();
    if (j.contains("ask")) e.ask = j.at("ask").get<double>();
    if (j.contains("last")) e.last = j.at("last").get<double>();
    if (j.contains("bid_size")) e.bid_size = j.at("bid_size").get<double>();
    if (j.contains("ask_size")) e.ask_size = j.at("ask_size").get<double>();
    if (j.contains("trade_size")) e.trade_size = j.at("trade_size").get<double>();
    return e;
}

json candle_to_json(const Candle& c) {
    return json{
        {"instrument", c.instrument},
        {"open_time", ts_count(c.open_time)},
        {"open", c.open},
        {"high", c.high},
        {"low", c.low},
        {"close", c.close},
        {"ticks", c.ticks},
        {"status", static_cast<int>(c.status)},
    };
}

Candle candle_from_json(const json& j) {
    Candle c;
    c.instrument = j.value("instrument", 0u);
    c.open_time = ts_from(j.value("open_time", 0ll));
    c.open = j.value("open", 0.0);
    c.high = j.value("high", 0.0);
    c.low = j.value("low", 0.0);
    c.close = j.value("close", 0.0);
    c.ticks = j.value("ticks", 0u);
    c.status = static_cast<CandleStatus>(j.value("status", 0));
    return c;
}

json position_to_json(const PositionState& p) {
    return json{
        {"id", p.id},
        {"intent_id", p.intent_id},
        {"instrument", p.instrument},
        {"direction", static_cast<int>(p.direction)},
        {"entry_price", p.entry_price},
        {"quantity", p.quantity},
        {"current_price", p.current_price},
        {"opened_at", ts_count(p.opened_at)},
        {"mfe", p.mfe},
        {"mae", p.mae},
        {"peak_favorable_price", p.peak_favorable_price},
        {"peak_retention", p.peak_retention},
        {"current_pnl", p.current_pnl},
        {"stop_loss", p.stop_loss},
        {"take_profit", p.take_profit},
        {"deal_id", p.deal_id},
        {"entry_thesis_quality", p.entry_thesis_quality},
        {"entry_continuation", p.entry_continuation},
        {"entry_invalidation", p.entry_invalidation},
    };
}

PositionState position_from_json(const json& j) {
    PositionState p;
    p.id = j.value("id", 0ull);
    p.intent_id = j.value("intent_id", 0ull);
    p.instrument = j.value("instrument", 0u);
    p.direction = static_cast<Direction>(j.value("direction", 0));
    p.entry_price = j.value("entry_price", 0.0);
    p.quantity = j.value("quantity", 0.0);
    p.current_price = j.value("current_price", 0.0);
    p.opened_at = ts_from(j.value("opened_at", 0ll));
    p.mfe = j.value("mfe", 0.0);
    p.mae = j.value("mae", 0.0);
    p.peak_favorable_price = j.value("peak_favorable_price", 0.0);
    p.peak_retention = j.value("peak_retention", 0.0);
    p.current_pnl = j.value("current_pnl", 0.0);
    p.stop_loss = j.value("stop_loss", 0.0);
    p.take_profit = j.value("take_profit", 0.0);
    p.deal_id = j.value("deal_id", "");
    p.entry_thesis_quality = j.value("entry_thesis_quality", 0.0);
    p.entry_continuation = j.value("entry_continuation", 0.0);
    p.entry_invalidation = j.value("entry_invalidation", 0.0);
    return p;
}

json decision_to_json(const PositionDecision& d) {
    return json{
        {"action", static_cast<int>(d.action)},
        {"reason", static_cast<int>(d.reason)},
        {"ev_exit", d.ev_exit},
        {"ev_hold", d.ev_hold},
        {"continuation_strength", d.continuation_strength},
        {"degradation", d.degradation},
        {"reduce_fraction", d.reduce_fraction},
        {"suggested_stop", d.suggested_stop},
        {"suggested_target", d.suggested_target},
        {"reason_codes", d.reason_codes},
    };
}

PositionDecision decision_from_json(const json& j) {
    PositionDecision d;
    d.action = static_cast<PositionAction>(j.value("action", 0));
    d.reason = static_cast<ExitReason>(j.value("reason", 0));
    d.ev_exit = j.value("ev_exit", 0.0);
    d.ev_hold = j.value("ev_hold", 0.0);
    d.continuation_strength = j.value("continuation_strength", 0.0);
    d.degradation = j.value("degradation", 0.0);
    d.reduce_fraction = j.value("reduce_fraction", 0.0);
    d.suggested_stop = j.value("suggested_stop", 0.0);
    d.suggested_target = j.value("suggested_target", 0.0);
    if (j.contains("reason_codes")) d.reason_codes = j.at("reason_codes").get<std::vector<std::string>>();
    return d;
}

json frame_to_json(const EpisodeBrainFrame& f) {
    return json{
        {"ts", ts_count(f.ts)},
        {"trigger", static_cast<int>(f.trigger)},
        {"instrument", f.instrument},
        {"prediction",
         {{"long_side", side_to_json(f.prediction.long_side)},
          {"short_side", side_to_json(f.prediction.short_side)},
          {"has_structure_authority", f.prediction.has_structure_authority},
          {"has_micro_authority", f.prediction.has_micro_authority},
          {"evidence_sufficient", f.prediction.evidence_sufficient},
          {"structure_volatility", f.prediction.structure_volatility},
          {"structure_invalidation", f.prediction.structure_invalidation}}},
        {"decision_action", static_cast<int>(f.decision_action)},
        {"decision",
         {{"instrument", f.decision.instrument},
          {"direction", static_cast<int>(f.decision.direction)},
          {"probability", f.decision.probability},
          {"expected_value", f.decision.expected_value},
          {"spread_cost", f.decision.spread_cost},
          {"action", static_cast<int>(f.decision.action)},
          {"stop_distance_frac", f.decision.stop_distance_frac},
          {"target_distance_frac", f.decision.target_distance_frac}}},
        {"risk",
         {{"approved", f.risk.approved},
          {"approved_quantity", f.risk.approved_quantity},
          {"confidence", f.risk.confidence},
          {"reason_codes", f.risk.reason_codes}}},
        {"execution",
         {{"status", static_cast<int>(f.execution.status)},
          {"requested_quantity", f.execution.requested_quantity},
          {"filled_quantity", f.execution.filled_quantity},
          {"fill_price", f.execution.fill_price},
          {"deal_id", f.execution.deal_id},
          {"explanation", f.execution.explanation}}},
        {"position", position_to_json(f.position)},
        {"position_decision", decision_to_json(f.position_decision)},
        {"has_structure_authority", f.has_structure_authority},
        {"has_micro_authority", f.has_micro_authority},
        {"has_prediction", f.has_prediction},
        {"has_decision", f.has_decision},
        {"has_risk", f.has_risk},
        {"has_execution", f.has_execution},
        {"has_position", f.has_position},
    };
}

EpisodeBrainFrame frame_from_json(const json& j) {
    EpisodeBrainFrame f;
    f.ts = ts_from(j.value("ts", 0ll));
    f.trigger = static_cast<EpisodeClockDomain>(j.value("trigger", 0));
    f.instrument = j.value("instrument", 0u);
    if (j.contains("prediction")) {
        const auto& p = j.at("prediction");
        if (p.contains("long_side")) f.prediction.long_side = side_from_json(p.at("long_side"));
        if (p.contains("short_side")) f.prediction.short_side = side_from_json(p.at("short_side"));
        f.prediction.has_structure_authority = p.value("has_structure_authority", false);
        f.prediction.has_micro_authority = p.value("has_micro_authority", false);
        f.prediction.evidence_sufficient = p.value("evidence_sufficient", false);
        f.prediction.structure_volatility = p.value("structure_volatility", 0.0);
        f.prediction.structure_invalidation = p.value("structure_invalidation", 0.0);
    }
    f.decision_action = static_cast<TradeAction>(j.value("decision_action", 0));
    if (j.contains("decision")) {
        const auto& d = j.at("decision");
        f.decision.instrument = d.value("instrument", 0u);
        f.decision.direction = static_cast<Direction>(d.value("direction", 0));
        f.decision.probability = d.value("probability", 0.0);
        f.decision.expected_value = d.value("expected_value", 0.0);
        f.decision.spread_cost = d.value("spread_cost", 0.0);
        f.decision.action = static_cast<TradeAction>(d.value("action", 0));
        f.decision.stop_distance_frac = d.value("stop_distance_frac", 0.0);
        f.decision.target_distance_frac = d.value("target_distance_frac", 0.0);
    }
    if (j.contains("risk")) {
        const auto& r = j.at("risk");
        f.risk.approved = r.value("approved", false);
        f.risk.approved_quantity = r.value("approved_quantity", 0.0);
        f.risk.confidence = r.value("confidence", 0.0);
        if (r.contains("reason_codes"))
            f.risk.reason_codes = r.at("reason_codes").get<std::vector<std::string>>();
    }
    if (j.contains("execution")) {
        const auto& e = j.at("execution");
        f.execution.status = static_cast<ExecutionStatus>(e.value("status", 0));
        f.execution.requested_quantity = e.value("requested_quantity", 0.0);
        f.execution.filled_quantity = e.value("filled_quantity", 0.0);
        f.execution.fill_price = e.value("fill_price", 0.0);
        f.execution.deal_id = e.value("deal_id", "");
        f.execution.explanation = e.value("explanation", "");
    }
    if (j.contains("position")) f.position = position_from_json(j.at("position"));
    if (j.contains("position_decision"))
        f.position_decision = decision_from_json(j.at("position_decision"));
    f.has_structure_authority = j.value("has_structure_authority", false);
    f.has_micro_authority = j.value("has_micro_authority", false);
    f.has_prediction = j.value("has_prediction", false);
    f.has_decision = j.value("has_decision", false);
    f.has_risk = j.value("has_risk", false);
    f.has_execution = j.value("has_execution", false);
    f.has_position = j.value("has_position", false);
    return f;
}

json episode_to_json(const TradeEpisode& ep) {
    json market = json::array();
    for (const auto& m : ep.market) {
        json item{
            {"domain", static_cast<int>(m.domain)},
            {"ts", ts_count(m.ts)},
            {"sequence", m.sequence},
            {"instrument", m.instrument},
            {"timeframe", static_cast<int>(m.timeframe)},
            {"structure_authority", m.structure_authority},
            {"one_shot", m.one_shot},
        };
        if (m.quote) item["quote"] = quote_to_json(*m.quote);
        if (m.candle) item["candle"] = candle_to_json(*m.candle);
        market.push_back(std::move(item));
    }

    json frames = json::array();
    for (const auto& f : ep.frames) frames.push_back(frame_to_json(f));

    json actions = json::array();
    for (const auto& a : ep.position_actions) actions.push_back(decision_to_json(a));

    return json{
        {"episode_id", ep.episode_id},
        {"instrument", ep.instrument},
        {"sealed", ep.sealed},
        {"provenance",
         {{"episode_schema_version", ep.provenance.episode_schema_version},
          {"brain_version", ep.provenance.brain_version},
          {"model_id", ep.provenance.model_id},
          {"config_hash", ep.provenance.config_hash},
          {"prediction_weights_hash", ep.provenance.prediction_weights_hash},
          {"decision_weights_hash", ep.provenance.decision_weights_hash},
          {"risk_weights_hash", ep.provenance.risk_weights_hash},
          {"execution_weights_hash", ep.provenance.execution_weights_hash},
          {"position_weights_hash", ep.provenance.position_weights_hash}}},
        {"market", std::move(market)},
        {"frames", std::move(frames)},
        {"position_actions", std::move(actions)},
        {"outcome",
         {{"sealed", ep.outcome.sealed},
          {"direction", static_cast<int>(ep.outcome.direction)},
          {"entry_price", ep.outcome.entry_price},
          {"exit_price", ep.outcome.exit_price},
          {"quantity", ep.outcome.quantity},
          {"realized_pnl", ep.outcome.realized_pnl},
          {"mfe", ep.outcome.mfe},
          {"mae", ep.outcome.mae},
          {"peak_retention", ep.outcome.peak_retention},
          {"exit_action", static_cast<int>(ep.outcome.exit_action)},
          {"exit_reason", static_cast<int>(ep.outcome.exit_reason)},
          {"entry_ts", ts_count(ep.outcome.entry_ts)},
          {"exit_ts", ts_count(ep.outcome.exit_ts)},
          {"post_exit_favorable", ep.outcome.post_exit_favorable},
          {"post_exit_adverse", ep.outcome.post_exit_adverse},
          {"post_exit_end_ts", ts_count(ep.outcome.post_exit_end_ts)},
          {"post_exit_samples", ep.outcome.post_exit_samples}}},
    };
}

TradeEpisode episode_from_json(const json& j) {
    TradeEpisode ep;
    ep.episode_id = j.value("episode_id", "");
    ep.instrument = j.value("instrument", 0u);
    ep.sealed = j.value("sealed", false);
    if (j.contains("provenance")) {
        const auto& p = j.at("provenance");
        ep.provenance.episode_schema_version = p.value("episode_schema_version", "vs-v2-episode-1");
        ep.provenance.brain_version = p.value("brain_version", std::string{kBrainVersion});
        ep.provenance.model_id = p.value("model_id", "default");
        ep.provenance.config_hash = p.value("config_hash", "");
        ep.provenance.prediction_weights_hash = p.value("prediction_weights_hash", "");
        ep.provenance.decision_weights_hash = p.value("decision_weights_hash", "");
        ep.provenance.risk_weights_hash = p.value("risk_weights_hash", "");
        ep.provenance.execution_weights_hash = p.value("execution_weights_hash", "");
        ep.provenance.position_weights_hash = p.value("position_weights_hash", "");
    }
    if (j.contains("market")) {
        for (const auto& item : j.at("market")) {
            EpisodeMarketItem m;
            m.domain = static_cast<EpisodeClockDomain>(item.value("domain", 0));
            m.ts = ts_from(item.value("ts", 0ll));
            m.sequence = item.value("sequence", 0ull);
            m.instrument = item.value("instrument", 0u);
            m.timeframe = static_cast<Timeframe>(item.value("timeframe", 1));
            m.structure_authority = item.value("structure_authority", false);
            m.one_shot = item.value("one_shot", false);
            if (item.contains("quote")) m.quote = quote_from_json(item.at("quote"));
            if (item.contains("candle")) m.candle = candle_from_json(item.at("candle"));
            ep.market.push_back(std::move(m));
        }
    }
    if (j.contains("frames")) {
        for (const auto& f : j.at("frames")) ep.frames.push_back(frame_from_json(f));
    }
    if (j.contains("position_actions")) {
        for (const auto& a : j.at("position_actions"))
            ep.position_actions.push_back(decision_from_json(a));
    }
    if (j.contains("outcome")) {
        const auto& o = j.at("outcome");
        ep.outcome.sealed = o.value("sealed", false);
        ep.outcome.direction = static_cast<Direction>(o.value("direction", 0));
        ep.outcome.entry_price = o.value("entry_price", 0.0);
        ep.outcome.exit_price = o.value("exit_price", 0.0);
        ep.outcome.quantity = o.value("quantity", 0.0);
        ep.outcome.realized_pnl = o.value("realized_pnl", 0.0);
        ep.outcome.mfe = o.value("mfe", 0.0);
        ep.outcome.mae = o.value("mae", 0.0);
        ep.outcome.peak_retention = o.value("peak_retention", 0.0);
        ep.outcome.exit_action = static_cast<PositionAction>(o.value("exit_action", 3));
        ep.outcome.exit_reason = static_cast<ExitReason>(o.value("exit_reason", 0));
        ep.outcome.entry_ts = ts_from(o.value("entry_ts", 0ll));
        ep.outcome.exit_ts = ts_from(o.value("exit_ts", 0ll));
        ep.outcome.post_exit_favorable = o.value("post_exit_favorable", 0.0);
        ep.outcome.post_exit_adverse = o.value("post_exit_adverse", 0.0);
        ep.outcome.post_exit_end_ts = ts_from(o.value("post_exit_end_ts", 0ll));
        ep.outcome.post_exit_samples = o.value("post_exit_samples", 0u);
    }
    return ep;
}

}  // namespace

EpisodeStore::EpisodeStore(std::filesystem::path root) : root_(std::move(root)) {
    std::filesystem::create_directories(root_);
}

std::filesystem::path EpisodeStore::path_for(const std::string& episode_id) const {
    return root_ / (episode_id + ".episode.json");
}

void EpisodeStore::save(const TradeEpisode& episode) const {
    if (episode.episode_id.empty()) {
        throw std::invalid_argument("TradeEpisode.episode_id required");
    }
    const auto path = path_for(episode.episode_id);
    std::ofstream out(path);
    if (!out) throw std::runtime_error("failed to write episode: " + path.string());
    out << episode_to_json(episode).dump(2);
}

TradeEpisode EpisodeStore::load(const std::string& episode_id) const {
    const auto path = path_for(episode_id);
    std::ifstream in(path);
    if (!in) throw std::runtime_error("failed to read episode: " + path.string());
    json j;
    in >> j;
    return episode_from_json(j);
}

bool EpisodeStore::exists(const std::string& episode_id) const {
    return std::filesystem::exists(path_for(episode_id));
}

std::vector<std::string> EpisodeStore::list_ids() const {
    std::vector<std::string> ids;
    if (!std::filesystem::exists(root_)) return ids;
    for (const auto& entry : std::filesystem::directory_iterator(root_)) {
        if (!entry.is_regular_file()) continue;
        const auto name = entry.path().filename().string();
        const std::string suffix = ".episode.json";
        if (name.size() > suffix.size()
            && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            ids.push_back(name.substr(0, name.size() - suffix.size()));
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

}  // namespace mr
