import { beforeEach, describe, expect, it } from 'vitest';
import {
  getBrainEvent,
  getBrainFeedStatus,
  getLiveBrainSnapshot,
  ingestLiveBrainSnapshot,
  normalizeLiveSnapshot,
  listBrainEvents,
  resetLiveBrainFeedForTests,
} from './liveBrainFeed.js';

const sample = {
  source: 'market-core',
  brain_version: 'vs-v2-1.0.0',
  model_id: 'stage9',
  model_version: '9.0.0',
  snapshot_id: 42,
  ts_ns: 1_700_000_000_000_000_000,
  operating_mode: 'LIVE',
  health: {
    market_core: 'ONLINE',
    feeds: 'ONLINE',
    execution: 'ONLINE',
    data: 'ONLINE',
  },
  instruments: [
    {
      instrument_id: 1,
      epic: 'EURUSD',
      symbol: 'EURUSD',
      quote: { bid: 1.1, ask: 1.1002, mid: 1.1001, spread: 0.0002, ts_ns: 1 },
      structure: {
        swing_state: 1,
        trend_direction: 'Up',
        trend_strength: 0.7,
        volatility: 0.1,
        structure_quality: 0.8,
        structural_invalidation: 0.1,
        compression: 0.2,
        expansion: 0.3,
        breakout_active: false,
        in_range: true,
        range_position: 0.4,
      },
      micro: {
        spread: 0.0002,
        momentum: 0.2,
        acceleration: 0.1,
        acceptance: 0.5,
        rejection: 0.1,
        reclaim: 0.0,
        body_pct: 0.6,
        candle_strength: 0.7,
        swing_state: 1,
        aggressive_buy_pressure: 0.55,
        aggressive_sell_pressure: 0.2,
      },
      prediction: {
        long_side: {
          direction: 'LONG',
          continuation: 0.6,
          reversal_failure: 0.2,
          expected_move: 0.01,
          adverse_move: 0.005,
          probability: 0.58,
          confidence: 0.7,
          expected_value: 0.004,
          invalidation: 0.1,
          thesis_quality: 0.65,
          uncertainty: 0.3,
        },
        short_side: {
          direction: 'SHORT',
          continuation: 0.3,
          reversal_failure: 0.4,
          expected_move: 0.008,
          adverse_move: 0.006,
          probability: 0.4,
          confidence: 0.5,
          expected_value: -0.001,
          invalidation: 0.2,
          thesis_quality: 0.4,
          uncertainty: 0.5,
        },
        has_structure_authority: true,
        has_micro_authority: true,
        evidence_sufficient: true,
        structure_volatility: 0.1,
        structure_invalidation: 0.1,
      },
      decision: {
        instrument_id: 1,
        direction: 'LONG',
        probability: 0.58,
        expected_value: 0.004,
        spread_cost: 0.0002,
        action: 'BUY',
        reason_codes: ['EV_EDGE'],
        stop_distance_frac: 0.002,
        target_distance_frac: 0.004,
      },
      decision_action: 'BUY',
      risk: {
        approved: true,
        approved_quantity: 1,
        reason_codes: ['OK'],
        exposure: 0,
        daily_pnl: 0,
        max_drawdown: 0,
        risk_budget_used: 0.1,
      },
      execution: {
        status: 'FILLED',
        deal_id: 'D1',
        fill_price: 1.1001,
        filled_quantity: 1,
        message: 'ok',
      },
      position: {
        instrument_id: 1,
        direction: 'LONG',
        quantity: 1,
        entry_price: 1.1001,
        current_price: 1.1005,
        unrealized_pnl: 4,
        realized_pnl: 0,
        mfe: 5,
        mae: 1,
        deal_id: 'D1',
        position_action: 'HOLD',
      },
      has_structure_authority: true,
      has_micro_authority: true,
      has_prediction: true,
      has_decision: true,
      has_risk: true,
      has_execution: true,
      has_position: true,
      ts_ns: 1,
    },
  ],
};

describe('liveBrainFeed', () => {
  beforeEach(() => {
    resetLiveBrainFeedForTests();
  });

  it('rejects non market-core sources (no invented brain)', () => {
    expect(() => ingestLiveBrainSnapshot({ ...sample, source: 'dashboard' })).toThrow(
      /market-core/,
    );
    expect(getLiveBrainSnapshot()).toBeNull();
    expect(getBrainFeedStatus().invents_decisions).toBe(false);
    expect(getBrainFeedStatus().authoritative).toBe(true);
  });

  it('stores authoritative snapshot and detail events', () => {
    const snap = ingestLiveBrainSnapshot(sample);
    expect(snap.snapshot_id).toBe(42);
    expect(snap.instruments[0].decision_action).toBe('BUY');
    expect(getLiveBrainSnapshot()?.model_id).toBe('stage9');

    const events = listBrainEvents({ limit: 50 });
    expect(events.some((e) => e.kind === 'snapshot')).toBe(true);
    expect(events.some((e) => e.kind === 'decision')).toBe(true);
    expect(events.some((e) => e.kind === 'prediction')).toBe(true);
    expect(events.some((e) => e.kind === 'trade')).toBe(true);
    expect(events.some((e) => e.kind === 'position')).toBe(true);

    const decision = events.find((e) => e.kind === 'decision')!;
    const full = getBrainEvent(decision.id);
    expect(full?.evidence).toBeTruthy();
    expect(full?.title).toContain('BUY');
  });
});


describe('liveBrainFeed unavailable metrics', () => {
  beforeEach(() => {
    resetLiveBrainFeedForTests();
  });

  it('preserves null risk/pnl and UNKNOWN health instead of inventing 0/ONLINE', () => {
    const snap = normalizeLiveSnapshot({
      source: 'market-core',
      brain_version: 'vs-v2-1.0.0',
      model_id: 'stage9',
      model_version: '9.0.1',
      snapshot_id: 99,
      ts_ns: 1,
      operating_mode: 'UNKNOWN',
      health: {
        market_core: 'UNKNOWN',
        feeds: 'UNKNOWN',
        execution: 'UNKNOWN',
        data: 'UNKNOWN',
      },
      instruments: [
        {
          instrument_id: 1,
          decision_action: 'WAIT',
          risk: {
            approved: false,
            approved_quantity: 0,
            reason_codes: [],
            exposure: null,
            daily_pnl: null,
            max_drawdown: null,
            risk_budget_used: 0,
          },
          position: {
            instrument_id: 1,
            direction: 'FLAT',
            quantity: 0,
            entry_price: 0,
            current_price: 0,
            unrealized_pnl: 0,
            realized_pnl: null,
            mfe: 0,
            mae: 0,
            deal_id: '',
            position_action: 'HOLD',
          },
        },
      ],
    });

    expect(snap.health.market_core).toBe('UNKNOWN');
    expect(snap.health.feeds).toBe('UNKNOWN');
    expect(snap.instruments[0].risk.exposure).toBeNull();
    expect(snap.instruments[0].risk.daily_pnl).toBeNull();
    expect(snap.instruments[0].risk.max_drawdown).toBeNull();
    expect(snap.instruments[0].position.realized_pnl).toBeNull();
    expect(snap.health.market_core).not.toBe('ONLINE');
  });

  it('does not coerce missing exposure/pnl keys to 0', () => {
    const snap = normalizeLiveSnapshot({
      source: 'market-core',
      snapshot_id: 1,
      ts_ns: 1,
      instruments: [{ instrument_id: 1, risk: { approved: true }, position: {} }],
    });
    expect(snap.instruments[0].risk.exposure).toBeNull();
    expect(snap.instruments[0].risk.daily_pnl).toBeNull();
    expect(snap.instruments[0].risk.max_drawdown).toBeNull();
    expect(snap.instruments[0].position.realized_pnl).toBeNull();
  });
});
