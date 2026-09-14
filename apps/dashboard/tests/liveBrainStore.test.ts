import { beforeEach, describe, expect, it, vi } from 'vitest';

vi.mock('../src/services/api', () => ({
  api: { get: vi.fn() },
}));

import { api } from '../src/services/api';
import { selectActiveInstrument, useLiveBrainStore } from '../src/state/liveBrainStore';

describe('liveBrainStore', () => {
  beforeEach(() => {
    useLiveBrainStore.setState({
      snapshot: null,
      status: null,
      events: [],
      selectedEvent: null,
      selectedInstrumentId: null,
      wsConnected: false,
      awaitingMarketCore: true,
      loading: false,
      error: null,
    });
    vi.mocked(api.get).mockReset();
  });

  it('applies authoritative snapshot without inventing actions', () => {
    useLiveBrainStore.getState().applySnapshot({
      source: 'market-core',
      brain_version: 'vs-v2-1.0.0',
      model_id: 'm',
      model_version: '1',
      snapshot_id: 7,
      ts_ns: 1,
      operating_mode: 'LIVE',
      health: { market_core: 'ONLINE', feeds: 'ONLINE', execution: 'ONLINE', data: 'ONLINE' },
      instruments: [
        {
          instrument_id: 9,
          quote: { bid: 1, ask: 1.1, mid: 1.05, spread: 0.1, ts_ns: 1 },
          structure: {
            swing_state: 0, trend_direction: 'Range', trend_strength: 0, volatility: 0,
            structure_quality: 0, structural_invalidation: 0, compression: 0, expansion: 0,
            breakout_active: false, in_range: true, range_position: 0.5,
          },
          micro: {
            spread: 0, momentum: 0, acceleration: 0, acceptance: 0, rejection: 0, reclaim: 0,
            body_pct: 0, candle_strength: 0, swing_state: 0,
            aggressive_buy_pressure: 0, aggressive_sell_pressure: 0,
          },
          prediction: {
            long_side: {
              direction: 'LONG', continuation: 0, reversal_failure: 0, expected_move: 0, adverse_move: 0,
              probability: 0, confidence: 0, expected_value: 0, invalidation: 0, thesis_quality: 0, uncertainty: 0,
            },
            short_side: {
              direction: 'SHORT', continuation: 0, reversal_failure: 0, expected_move: 0, adverse_move: 0,
              probability: 0, confidence: 0, expected_value: 0, invalidation: 0, thesis_quality: 0, uncertainty: 0,
            },
            has_structure_authority: false, has_micro_authority: false, evidence_sufficient: false,
            structure_volatility: 0, structure_invalidation: 0,
          },
          decision: {
            instrument_id: 9, direction: 'FLAT', probability: 0, expected_value: 0, spread_cost: 0,
            action: 'WAIT', reason_codes: [], stop_distance_frac: 0, target_distance_frac: 0,
          },
          decision_action: 'WAIT',
          risk: { approved: false, approved_quantity: 0, reason_codes: [], exposure: 0, daily_pnl: 0, max_drawdown: 0, risk_budget_used: 0 },
          execution: { status: 'NONE', deal_id: '', fill_price: 0, filled_quantity: 0, message: '' },
          position: {
            instrument_id: 9, direction: 'FLAT', quantity: 0, entry_price: 0, current_price: 0,
            unrealized_pnl: 0, realized_pnl: 0, mfe: 0, mae: 0, deal_id: '', position_action: 'HOLD',
          },
          has_structure_authority: false, has_micro_authority: false, has_prediction: false,
          has_decision: true, has_risk: false, has_execution: false, has_position: false, ts_ns: 1,
        },
      ],
    });
    const state = useLiveBrainStore.getState();
    expect(state.awaitingMarketCore).toBe(false);
    expect(state.status?.invents_decisions).toBe(false);
    expect(state.status?.authoritative).toBe(true);
    expect(selectActiveInstrument(state)?.decision_action).toBe('WAIT');
  });

  it('loads live feed from control-api', async () => {
    vi.mocked(api.get).mockResolvedValueOnce({
      snapshot: null,
      status: {
        connected: false, ingest_count: 0, last_ingest_ms: null, age_ms: null, last_error: null,
        brain_version: null, model_id: null, model_version: null, instrument_count: 0, source: null,
        authoritative: true, invents_decisions: false,
      },
      awaiting_market_core: true,
    });
    await useLiveBrainStore.getState().fetchLive();
    expect(api.get).toHaveBeenCalledWith('/api/brain/live');
    expect(useLiveBrainStore.getState().awaitingMarketCore).toBe(true);
  });
});

describe('liveBrainStore unavailable telemetry', () => {
  beforeEach(() => {
    useLiveBrainStore.setState({
      snapshot: null,
      status: null,
      events: [],
      selectedEvent: null,
      selectedInstrumentId: null,
      wsConnected: false,
      awaitingMarketCore: true,
      loading: false,
      error: null,
    });
  });

  it('does not present null risk/pnl as real zeros or ONLINE health', () => {
    useLiveBrainStore.getState().applySnapshot({
      source: 'market-core',
      brain_version: 'vs-v2-1.0.0',
      model_id: 'm',
      model_version: '1',
      snapshot_id: 8,
      ts_ns: 1,
      operating_mode: 'UNKNOWN',
      health: { market_core: 'UNKNOWN', feeds: 'UNKNOWN', execution: 'UNKNOWN', data: 'UNKNOWN' },
      instruments: [
        {
          instrument_id: 9,
          quote: { bid: 0, ask: 0, mid: 0, spread: 0, ts_ns: 1 },
          structure: {
            swing_state: 0, trend_direction: 'Range', trend_strength: 0, volatility: 0,
            structure_quality: 0, structural_invalidation: 0, compression: 0, expansion: 0,
            breakout_active: false, in_range: true, range_position: 0.5,
          },
          micro: {
            spread: 0, momentum: 0, acceleration: 0, acceptance: 0, rejection: 0, reclaim: 0,
            body_pct: 0, candle_strength: 0, swing_state: 0,
            aggressive_buy_pressure: 0, aggressive_sell_pressure: 0,
          },
          prediction: {
            long_side: {
              direction: 'LONG', continuation: 0, reversal_failure: 0, expected_move: 0, adverse_move: 0,
              probability: 0, confidence: 0, expected_value: 0, invalidation: 0, thesis_quality: 0, uncertainty: 0,
            },
            short_side: {
              direction: 'SHORT', continuation: 0, reversal_failure: 0, expected_move: 0, adverse_move: 0,
              probability: 0, confidence: 0, expected_value: 0, invalidation: 0, thesis_quality: 0, uncertainty: 0,
            },
            has_structure_authority: false, has_micro_authority: false, evidence_sufficient: false,
            structure_volatility: 0, structure_invalidation: 0,
          },
          decision: {
            instrument_id: 9, direction: 'FLAT', probability: 0, expected_value: 0, spread_cost: 0,
            action: 'WAIT', reason_codes: [], stop_distance_frac: 0, target_distance_frac: 0,
          },
          decision_action: 'WAIT',
          risk: {
            approved: false, approved_quantity: 0, reason_codes: [],
            exposure: null, daily_pnl: null, max_drawdown: null, risk_budget_used: 0,
          },
          execution: { status: 'NONE', deal_id: '', fill_price: 0, filled_quantity: 0, message: '' },
          position: {
            instrument_id: 9, direction: 'FLAT', quantity: 0, entry_price: 0, current_price: 0,
            unrealized_pnl: 0, realized_pnl: null, mfe: 0, mae: 0, deal_id: '', position_action: 'HOLD',
          },
          has_structure_authority: false, has_micro_authority: false, has_prediction: false,
          has_decision: true, has_risk: false, has_execution: false, has_position: false, ts_ns: 1,
        },
      ],
    });

    const state = useLiveBrainStore.getState();
    const inst = selectActiveInstrument(state);
    expect(inst?.risk.exposure).toBeNull();
    expect(inst?.risk.daily_pnl).toBeNull();
    expect(inst?.risk.max_drawdown).toBeNull();
    expect(inst?.position.realized_pnl).toBeNull();
    expect(state.snapshot?.health.market_core).toBe('UNKNOWN');
    expect(state.snapshot?.health.market_core).not.toBe('ONLINE');
  });
});
