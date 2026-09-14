/** Authoritative LIVE Brain shapes — mirror C++ BrainContext / BrainSnapshot. */
export type TradeAction = 'WAIT' | 'BUY' | 'SELL';
export type Direction = 'FLAT' | 'LONG' | 'SHORT';

export interface SidePredictionView {
  direction: Direction;
  continuation: number;
  reversal_failure: number;
  expected_move: number;
  adverse_move: number;
  probability: number;
  confidence: number;
  expected_value: number;
  invalidation: number;
  thesis_quality: number;
  uncertainty: number;
}

export interface DualPredictionView {
  long_side: SidePredictionView;
  short_side: SidePredictionView;
  has_structure_authority: boolean;
  has_micro_authority: boolean;
  evidence_sufficient: boolean;
  structure_volatility: number;
  structure_invalidation: number;
}

export interface StructureView {
  swing_state: number;
  trend_direction: string;
  trend_strength: number;
  volatility: number;
  structure_quality: number;
  structural_invalidation: number;
  compression: number;
  expansion: number;
  breakout_active: boolean;
  in_range: boolean;
  range_position: number;
}

export interface MicrostructureView {
  spread: number;
  momentum: number;
  acceleration: number;
  acceptance: number;
  rejection: number;
  reclaim: number;
  body_pct: number;
  candle_strength: number;
  swing_state: number;
  aggressive_buy_pressure: number;
  aggressive_sell_pressure: number;
}

export interface OpportunityView {
  instrument_id: number;
  direction: Direction;
  probability: number;
  expected_value: number;
  spread_cost: number;
  action: TradeAction;
  reason_codes: string[];
  stop_distance_frac: number;
  target_distance_frac: number;
}

export interface RiskView {
  approved: boolean;
  approved_quantity: number;
  reason_codes: string[];
  exposure: number | null;
  daily_pnl: number | null;
  max_drawdown: number | null;
  risk_budget_used: number;
}

export interface ExecutionView {
  status: string;
  deal_id: string;
  fill_price: number;
  filled_quantity: number;
  message: string;
}

export interface PositionView {
  instrument_id: number;
  direction: Direction;
  quantity: number;
  entry_price: number;
  current_price: number;
  unrealized_pnl: number;
  realized_pnl: number | null;
  mfe: number;
  mae: number;
  deal_id: string;
  position_action: string;
}

export interface QuoteView {
  bid: number;
  ask: number;
  mid: number;
  spread: number;
  ts_ns: number;
}

export interface InstrumentBrainView {
  instrument_id: number;
  epic?: string;
  symbol?: string;
  quote: QuoteView;
  structure: StructureView;
  micro: MicrostructureView;
  prediction: DualPredictionView;
  decision: OpportunityView;
  decision_action: TradeAction;
  risk: RiskView;
  execution: ExecutionView;
  position: PositionView;
  has_structure_authority: boolean;
  has_micro_authority: boolean;
  has_prediction: boolean;
  has_decision: boolean;
  has_risk: boolean;
  has_execution: boolean;
  has_position: boolean;
  ts_ns: number;
}

export interface LiveBrainSnapshot {
  source: 'market-core';
  brain_version: string;
  model_id: string;
  model_version: string;
  snapshot_id: number;
  ts_ns: number;
  operating_mode: string;
  health: {
    market_core: string;
    feeds: string;
    execution: string;
    data: string;
  };
  instruments: InstrumentBrainView[];
}

export type BrainEventKind =
  | 'snapshot'
  | 'decision'
  | 'prediction'
  | 'trade'
  | 'position'
  | 'market'
  | 'risk'
  | 'execution'
  | 'error';

export interface BrainEventRecord {
  id: string;
  kind: BrainEventKind;
  ts_ns: number;
  instrument_id?: number;
  epic?: string;
  title: string;
  summary: string;
  evidence: Record<string, unknown>;
  related_ids: string[];
}
