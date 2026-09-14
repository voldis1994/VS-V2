/**
 * Authoritative LIVE Brain feed — stores/forwards C++ market-core snapshots.
 * Does NOT invent TradeAction / predictions / structure.
 */
import { randomUUID } from 'node:crypto';
import { brainStream } from '../websocket/brainStream.js';
import type {
  BrainEventKind,
  BrainEventRecord,
  InstrumentBrainView,
  LiveBrainSnapshot,
  TradeAction,
} from '../types/liveBrain.js';

const MAX_EVENTS = 2_000;

let latest: LiveBrainSnapshot | null = null;
const events: BrainEventRecord[] = [];
const eventsById = new Map<string, BrainEventRecord>();
let ingestCount = 0;
let lastIngestMs = 0;
let lastError: string | null = null;

function pushEvent(ev: BrainEventRecord): void {
  events.unshift(ev);
  eventsById.set(ev.id, ev);
  while (events.length > MAX_EVENTS) {
    const dropped = events.pop();
    if (dropped) eventsById.delete(dropped.id);
  }
}

function recordFromInstrument(
  snap: LiveBrainSnapshot,
  inst: InstrumentBrainView,
  kinds: BrainEventKind[]
): void {
  const related: string[] = [];
  for (const kind of kinds) {
    const id = randomUUID();
    related.push(id);
    let title = String(kind);
    let summary = '';
    let evidence: Record<string, unknown> = {
      instrument: inst,
      snapshot_id: snap.snapshot_id,
    };

    switch (kind) {
      case 'decision':
        title = `DECISION ${inst.decision_action}`;
        summary = `${inst.symbol ?? inst.epic ?? inst.instrument_id} → ${inst.decision_action} EV=${inst.decision.expected_value.toFixed(4)} p=${inst.decision.probability.toFixed(3)}`;
        evidence = {
          decision: inst.decision,
          decision_action: inst.decision_action,
          reason_codes: inst.decision.reason_codes,
        };
        break;
      case 'prediction':
        title = 'PREDICTION LONG/SHORT';
        summary = `LONG EV=${inst.prediction.long_side.expected_value.toFixed(4)} SHORT EV=${inst.prediction.short_side.expected_value.toFixed(4)}`;
        evidence = { prediction: inst.prediction };
        break;
      case 'position':
        title = `POSITION ${inst.position.direction}`;
        summary = `qty=${inst.position.quantity} uPnL=${inst.position.unrealized_pnl.toFixed(2)} action=${inst.position.position_action}`;
        evidence = { position: inst.position };
        break;
      case 'trade':
        title = `TRADE ${inst.execution.status}`;
        summary = `${inst.execution.deal_id || '—'} fill=${inst.execution.fill_price} qty=${inst.execution.filled_quantity}`;
        evidence = { execution: inst.execution, decision: inst.decision };
        break;
      case 'market':
        title = 'MARKET QUOTE';
        summary = `mid=${inst.quote.mid} spread=${inst.quote.spread}`;
        evidence = { quote: inst.quote, structure: inst.structure, micro: inst.micro };
        break;
      case 'risk':
        title = inst.risk.approved ? 'RISK APPROVED' : 'RISK VETO';
        summary = `qty=${inst.risk.approved_quantity} exposure=${inst.risk.exposure}`;
        evidence = { risk: inst.risk };
        break;
      case 'execution':
        title = `EXEC ${inst.execution.status}`;
        summary = inst.execution.message || inst.execution.status;
        evidence = { execution: inst.execution };
        break;
      default:
        break;
    }

    pushEvent({
      id,
      kind,
      ts_ns: inst.ts_ns || snap.ts_ns,
      instrument_id: inst.instrument_id,
      epic: inst.epic,
      title,
      summary,
      evidence,
      related_ids: [...related],
    });
  }
}

function normalizeAction(raw: unknown): TradeAction {
  const s = String(raw ?? 'WAIT').toUpperCase();
  if (s === 'BUY' || s === 'SELL' || s === 'WAIT') return s;
  return 'WAIT';
}

export function normalizeLiveSnapshot(input: unknown): LiveBrainSnapshot {
  const body = (input ?? {}) as Record<string, unknown>;
  const src = String(body.source || 'market-core');
  if (src !== 'market-core') {
    throw new Error('Live brain snapshot source must be market-core');
  }
  const instrumentsIn = Array.isArray(body.instruments) ? body.instruments : [];
  const instruments: InstrumentBrainView[] = instrumentsIn.map((raw, idx) => {
    const r = (raw ?? {}) as Record<string, unknown>;
    const prediction = (r.prediction ?? {}) as Record<string, unknown>;
    const longSide = (prediction.long_side ?? {}) as Record<string, unknown>;
    const shortSide = (prediction.short_side ?? {}) as Record<string, unknown>;
    const decision = (r.decision ?? {}) as Record<string, unknown>;
    const structure = (r.structure ?? {}) as Record<string, unknown>;
    const micro = (r.micro ?? {}) as Record<string, unknown>;
    const risk = (r.risk ?? {}) as Record<string, unknown>;
    const execution = (r.execution ?? {}) as Record<string, unknown>;
    const position = (r.position ?? {}) as Record<string, unknown>;
    const quote = (r.quote ?? {}) as Record<string, unknown>;
    const action = normalizeAction(r.decision_action ?? decision.action);

    const side = (s: Record<string, unknown>, fallbackDir: 'LONG' | 'SHORT') => ({
      direction: String(s.direction || fallbackDir).toUpperCase() as 'FLAT' | 'LONG' | 'SHORT',
      continuation: Number(s.continuation || 0),
      reversal_failure: Number(s.reversal_failure || 0),
      expected_move: Number(s.expected_move || 0),
      adverse_move: Number(s.adverse_move || 0),
      probability: Number(s.probability || 0),
      confidence: Number(s.confidence || 0),
      expected_value: Number(s.expected_value || 0),
      invalidation: Number(s.invalidation || 0),
      thesis_quality: Number(s.thesis_quality || 0),
      uncertainty: Number(s.uncertainty || 0),
    });

    return {
      instrument_id: Number(r.instrument_id ?? idx + 1),
      epic: r.epic ? String(r.epic) : undefined,
      symbol: r.symbol ? String(r.symbol) : undefined,
      quote: {
        bid: Number(quote.bid || 0),
        ask: Number(quote.ask || 0),
        mid: Number(quote.mid || 0),
        spread: Number(quote.spread || 0),
        ts_ns: Number(quote.ts_ns || r.ts_ns || 0),
      },
      structure: {
        swing_state: Number(structure.swing_state || 0),
        trend_direction: String(structure.trend_direction || 'Unknown'),
        trend_strength: Number(structure.trend_strength || 0),
        volatility: Number(structure.volatility || 0),
        structure_quality: Number(structure.structure_quality || 0),
        structural_invalidation: Number(structure.structural_invalidation || 0),
        compression: Number(structure.compression || 0),
        expansion: Number(structure.expansion || 0),
        breakout_active: Boolean(structure.breakout_active),
        in_range: Boolean(structure.in_range),
        range_position: Number(structure.range_position || 0.5),
      },
      micro: {
        spread: Number(micro.spread || 0),
        momentum: Number(micro.momentum || 0),
        acceleration: Number(micro.acceleration || 0),
        acceptance: Number(micro.acceptance || 0),
        rejection: Number(micro.rejection || 0),
        reclaim: Number(micro.reclaim || 0),
        body_pct: Number(micro.body_pct || 0),
        candle_strength: Number(micro.candle_strength || 0),
        swing_state: Number(micro.swing_state || 0),
        aggressive_buy_pressure: Number(micro.aggressive_buy_pressure || 0),
        aggressive_sell_pressure: Number(micro.aggressive_sell_pressure || 0),
      },
      prediction: {
        long_side: side(longSide, 'LONG'),
        short_side: side(shortSide, 'SHORT'),
        has_structure_authority: Boolean(prediction.has_structure_authority ?? r.has_structure_authority),
        has_micro_authority: Boolean(prediction.has_micro_authority ?? r.has_micro_authority),
        evidence_sufficient: Boolean(prediction.evidence_sufficient),
        structure_volatility: Number(prediction.structure_volatility || 0),
        structure_invalidation: Number(prediction.structure_invalidation || 0),
      },
      decision: {
        instrument_id: Number(decision.instrument_id ?? r.instrument_id ?? idx + 1),
        direction: String(decision.direction || 'FLAT').toUpperCase() as 'FLAT' | 'LONG' | 'SHORT',
        probability: Number(decision.probability || 0),
        expected_value: Number(decision.expected_value || 0),
        spread_cost: Number(decision.spread_cost || 0),
        action,
        reason_codes: Array.isArray(decision.reason_codes) ? decision.reason_codes.map(String) : [],
        stop_distance_frac: Number(decision.stop_distance_frac || 0),
        target_distance_frac: Number(decision.target_distance_frac || 0),
      },
      decision_action: action,
      risk: {
        approved: Boolean(risk.approved),
        approved_quantity: Number(risk.approved_quantity || 0),
        reason_codes: Array.isArray(risk.reason_codes) ? risk.reason_codes.map(String) : [],
        exposure: Number(risk.exposure || 0),
        daily_pnl: Number(risk.daily_pnl || 0),
        max_drawdown: Number(risk.max_drawdown || 0),
        risk_budget_used: Number(risk.risk_budget_used || 0),
      },
      execution: {
        status: String(execution.status || 'NONE'),
        deal_id: String(execution.deal_id || ''),
        fill_price: Number(execution.fill_price || 0),
        filled_quantity: Number(execution.filled_quantity || 0),
        message: String(execution.message || ''),
      },
      position: {
        instrument_id: Number(position.instrument_id ?? r.instrument_id ?? idx + 1),
        direction: String(position.direction || 'FLAT').toUpperCase() as 'FLAT' | 'LONG' | 'SHORT',
        quantity: Number(position.quantity || 0),
        entry_price: Number(position.entry_price || 0),
        current_price: Number(position.current_price || 0),
        unrealized_pnl: Number(position.unrealized_pnl || 0),
        realized_pnl: Number(position.realized_pnl || 0),
        mfe: Number(position.mfe || 0),
        mae: Number(position.mae || 0),
        deal_id: String(position.deal_id || ''),
        position_action: String(position.position_action || 'HOLD'),
      },
      has_structure_authority: Boolean(r.has_structure_authority),
      has_micro_authority: Boolean(r.has_micro_authority),
      has_prediction: Boolean(r.has_prediction),
      has_decision: Boolean(r.has_decision),
      has_risk: Boolean(r.has_risk),
      has_execution: Boolean(r.has_execution),
      has_position: Boolean(r.has_position),
      ts_ns: Number(r.ts_ns || body.ts_ns || 0),
    };
  });

  const health = (body.health ?? {}) as Record<string, unknown>;
  return {
    source: 'market-core',
    brain_version: String(body.brain_version || 'vs-v2-1.0.0'),
    model_id: String(body.model_id || 'default'),
    model_version: String(body.model_version || '0.0.0'),
    snapshot_id: Number(body.snapshot_id || Date.now()),
    ts_ns: Number(body.ts_ns || 0),
    operating_mode: String(body.operating_mode || 'UNKNOWN'),
    health: {
      market_core: String(health.market_core || 'UNKNOWN'),
      feeds: String(health.feeds || 'UNKNOWN'),
      execution: String(health.execution || 'UNKNOWN'),
      data: String(health.data || 'UNKNOWN'),
    },
    instruments,
  };
}

export function ingestLiveBrainSnapshot(input: unknown): LiveBrainSnapshot {
  const snap = normalizeLiveSnapshot(input);
  latest = snap;
  ingestCount += 1;
  lastIngestMs = Date.now();
  lastError = null;

  pushEvent({
    id: randomUUID(),
    kind: 'snapshot',
    ts_ns: snap.ts_ns,
    title: `BRAIN SNAPSHOT #${snap.snapshot_id}`,
    summary: `${snap.instruments.length} instruments · mode=${snap.operating_mode} · model=${snap.model_id}@${snap.model_version}`,
    evidence: snap as unknown as Record<string, unknown>,
    related_ids: [],
  });

  for (const inst of snap.instruments) {
    const kinds: BrainEventKind[] = ['market', 'prediction'];
    if (inst.has_decision) kinds.push('decision');
    if (inst.has_risk) kinds.push('risk');
    if (inst.has_execution && inst.execution.status && inst.execution.status !== 'NONE') {
      kinds.push('execution', 'trade');
    }
    if (inst.has_position && inst.position.quantity > 0) kinds.push('position');
    recordFromInstrument(snap, inst, kinds);
  }

  brainStream.broadcast({ type: 'brain.live', snapshot: snap });
  return snap;
}

export function recordBrainError(message: string, evidence: Record<string, unknown> = {}): void {
  lastError = message;
  pushEvent({
    id: randomUUID(),
    kind: 'error',
    ts_ns: Date.now() * 1_000_000,
    title: 'BRAIN FEED ERROR',
    summary: message,
    evidence: { message, ...evidence },
    related_ids: [],
  });
  brainStream.broadcast({ type: 'brain.error', message, evidence });
}

export function getLiveBrainSnapshot(): LiveBrainSnapshot | null {
  return latest;
}

export function getBrainFeedStatus() {
  const ageMs = lastIngestMs > 0 ? Date.now() - lastIngestMs : null;
  return {
    connected: latest !== null && ageMs !== null && ageMs < 30_000,
    ingest_count: ingestCount,
    last_ingest_ms: lastIngestMs || null,
    age_ms: ageMs,
    last_error: lastError,
    brain_version: latest?.brain_version ?? null,
    model_id: latest?.model_id ?? null,
    model_version: latest?.model_version ?? null,
    instrument_count: latest?.instruments.length ?? 0,
    source: latest?.source ?? null,
    authoritative: true,
    invents_decisions: false,
  };
}

export function listBrainEvents(opts: {
  kind?: BrainEventKind | 'all';
  limit?: number;
  instrument_id?: number;
} = {}): BrainEventRecord[] {
  const limit = Math.min(Math.max(opts.limit ?? 100, 1), 500);
  return events
    .filter((e) => (opts.kind && opts.kind !== 'all' ? e.kind === opts.kind : true))
    .filter((e) => (opts.instrument_id != null ? e.instrument_id === opts.instrument_id : true))
    .slice(0, limit);
}

export function getBrainEvent(id: string): BrainEventRecord | null {
  return eventsById.get(id) ?? null;
}

export function resetLiveBrainFeedForTests(): void {
  latest = null;
  events.length = 0;
  eventsById.clear();
  ingestCount = 0;
  lastIngestMs = 0;
  lastError = null;
}
