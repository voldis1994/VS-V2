import { beforeEach, describe, expect, it, vi } from 'vitest';

vi.mock('./audit.js', () => ({
  logAudit: vi.fn(async () => undefined),
}));

vi.mock('./liveBrainFeed.js', () => ({
  getLiveBrainSnapshot: vi.fn(() => null),
  getBrainFeedStatus: vi.fn(() => ({
    connected: false,
    model_id: null,
    model_version: null,
  })),
}));

vi.mock('./pipelineBridge.js', () => ({
  getPipelineBridgeStatus: vi.fn(() => ({
    healthy: false,
    last_error: null,
  })),
}));

vi.mock('../config/environment.js', () => ({
  marketCoreAuthoritative: vi.fn(() => true),
}));

import { logAudit } from './audit.js';
import { getBrainFeedStatus, getLiveBrainSnapshot } from './liveBrainFeed.js';
import { getPipelineBridgeStatus } from './pipelineBridge.js';
import {
  evaluateLiveReadiness,
  getRuntimeModeState,
  liveEntriesAllowed,
  manageOpenPositionsAllowed,
  resetRuntimeModeForTests,
  switchRuntimeMode,
} from './runtimeMode.js';

function healthySnapshot(operatingMode = 'SHADOW') {
  return {
    source: 'market-core' as const,
    brain_version: 'v2',
    model_id: 'prod-model',
    model_version: '1.2.3',
    snapshot_id: 1,
    ts_ns: 1,
    operating_mode: operatingMode,
    health: {
      market_core: 'HEALTHY',
      feeds: 'HEALTHY',
      execution: 'HEALTHY',
      data: 'HEALTHY',
    },
    instruments: [
      {
        instrument_id: 1,
        has_risk: true,
        risk: {
          approved: true,
          approved_quantity: 1,
          reason_codes: [] as string[],
          exposure: 0,
          daily_pnl: 0,
          max_drawdown: 0,
          risk_budget_used: 0,
        },
      },
    ],
  };
}

describe('runtimeMode', () => {
  beforeEach(() => {
    resetRuntimeModeForTests();
    vi.mocked(getLiveBrainSnapshot).mockReturnValue(null as never);
    vi.mocked(getBrainFeedStatus).mockReturnValue({
      connected: false,
      model_id: null,
      model_version: null,
    } as never);
    vi.mocked(getPipelineBridgeStatus).mockReturnValue({
      healthy: false,
      last_error: null,
    } as never);
    vi.mocked(logAudit).mockClear();
  });

  it('defaults fail-closed: PAPER, no LIVE entries', () => {
    const state = getRuntimeModeState();
    expect(state.mode).toBe('PAPER');
    expect(liveEntriesAllowed()).toBe(false);
    expect(manageOpenPositionsAllowed()).toBe(false);
  });

  it('rejects LIVE without confirmation', async () => {
    const result = await switchRuntimeMode({ mode: 'LIVE', actor: 'ops' });
    expect(result.ok).toBe(false);
    if (!result.ok) expect(result.error).toMatch(/confirm/i);
    expect(getRuntimeModeState().mode).toBe('PAPER');
    expect(liveEntriesAllowed()).toBe(false);
  });

  it('rejects LIVE when health is not ready (fail-closed)', async () => {
    const result = await switchRuntimeMode({
      mode: 'LIVE',
      confirm: true,
      actor: 'ops',
    });
    expect(result.ok).toBe(false);
    if (!result.ok) {
      expect(result.error).toMatch(/fail-closed|blocked/i);
      expect(result.health?.ok).toBe(false);
    }
    expect(liveEntriesAllowed()).toBe(false);
    expect(logAudit).not.toHaveBeenCalled();
  });

  it('SHADOW→LIVE arms real execution when healthy + confirmed', async () => {
    await switchRuntimeMode({ mode: 'SHADOW', actor: 'ops' });
    expect(getRuntimeModeState().mode).toBe('SHADOW');
    expect(liveEntriesAllowed()).toBe(false);
    expect(manageOpenPositionsAllowed()).toBe(true);

    vi.mocked(getLiveBrainSnapshot).mockReturnValue(healthySnapshot('SHADOW') as never);
    vi.mocked(getBrainFeedStatus).mockReturnValue({
      connected: true,
      model_id: 'prod-model',
      model_version: '1.2.3',
    } as never);
    vi.mocked(getPipelineBridgeStatus).mockReturnValue({
      healthy: true,
      last_error: null,
    } as never);

    expect(evaluateLiveReadiness().ok).toBe(true);

    const result = await switchRuntimeMode({
      mode: 'LIVE',
      confirm: true,
      actor: 'alice',
    });
    expect(result.ok).toBe(true);
    expect(getRuntimeModeState().mode).toBe('LIVE');
    // C++ authoritative feed reflects LIVE after switch.
    vi.mocked(getLiveBrainSnapshot).mockReturnValue(healthySnapshot('LIVE') as never);
    expect(liveEntriesAllowed()).toBe(true);
    expect(manageOpenPositionsAllowed()).toBe(true);
    expect(logAudit).toHaveBeenCalled();
    const auditCall = vi.mocked(logAudit).mock.calls.at(-1)!;
    expect(auditCall[0]).toBe('alice');
    expect(auditCall[1]).toBe('runtime_mode_switch');
    expect(auditCall[5]).toMatchObject({ mode: 'LIVE' });
  });

  it('LIVE→SHADOW blocks new LIVE entries but keeps manage/exit', async () => {
    vi.mocked(getLiveBrainSnapshot).mockReturnValue(healthySnapshot('LIVE') as never);
    vi.mocked(getBrainFeedStatus).mockReturnValue({
      connected: true,
      model_id: 'prod-model',
      model_version: '1.2.3',
    } as never);
    vi.mocked(getPipelineBridgeStatus).mockReturnValue({
      healthy: true,
      last_error: null,
    } as never);

    const armed = await switchRuntimeMode({
      mode: 'LIVE',
      confirm: true,
      actor: 'bob',
    });
    expect(armed.ok).toBe(true);
    expect(liveEntriesAllowed()).toBe(true);

    const demoted = await switchRuntimeMode({
      mode: 'SHADOW',
      actor: 'bob',
    });
    expect(demoted.ok).toBe(true);
    expect(getRuntimeModeState().mode).toBe('SHADOW');
    expect(liveEntriesAllowed()).toBe(false);
    expect(manageOpenPositionsAllowed()).toBe(true);
    expect(process.env.LIVE_TRADING_ENABLED).toBe('false');
  });

  it('records who/when switched in audit + last_switch', async () => {
    await switchRuntimeMode({ mode: 'SHADOW', actor: 'carol' });
    const state = getRuntimeModeState();
    expect(state.last_switch.actor).toBe('carol');
    expect(state.last_switch.to).toBe('SHADOW');
    expect(state.last_switch.at).toBeTruthy();
    expect(logAudit).toHaveBeenCalledWith(
      'carol',
      'runtime_mode_switch',
      'system',
      'OPERATING_MODE',
      expect.anything(),
      expect.objectContaining({ mode: 'SHADOW' }),
    );
  });

  it('C++ disconnect/restart: LIVE entries fail-closed until feed returns LIVE', async () => {
    vi.mocked(getLiveBrainSnapshot).mockReturnValue(healthySnapshot('LIVE') as never);
    vi.mocked(getBrainFeedStatus).mockReturnValue({
      connected: true,
      model_id: 'prod-model',
      model_version: '1.2.3',
    } as never);
    vi.mocked(getPipelineBridgeStatus).mockReturnValue({
      healthy: true,
      last_error: null,
    } as never);

    const armed = await switchRuntimeMode({
      mode: 'LIVE',
      confirm: true,
      actor: 'ops',
    });
    expect(armed.ok).toBe(true);
    expect(liveEntriesAllowed()).toBe(true);
    expect(getRuntimeModeState().authoritative_mode).toBe('LIVE');

    // Feed drops (market-core restart / disconnect) — entries must block immediately.
    vi.mocked(getBrainFeedStatus).mockReturnValue({
      connected: false,
      model_id: null,
      model_version: null,
    } as never);
    vi.mocked(getLiveBrainSnapshot).mockReturnValue(null as never);
    expect(liveEntriesAllowed()).toBe(false);
    expect(getRuntimeModeState().authoritative_mode).toBe('UNKNOWN');
    // Existing positions may still be managed while control-api remains LIVE-armed.
    expect(manageOpenPositionsAllowed()).toBe(true);

    // Snapshot present but mode UNKNOWN / unparseable — still fail-closed.
    vi.mocked(getBrainFeedStatus).mockReturnValue({
      connected: true,
      model_id: 'prod-model',
      model_version: '1.2.3',
    } as never);
    vi.mocked(getLiveBrainSnapshot).mockReturnValue(healthySnapshot('UNKNOWN') as never);
    expect(liveEntriesAllowed()).toBe(false);
    expect(getRuntimeModeState().authoritative_mode).toBe('UNKNOWN');

    // Connected but C++ still SHADOW after restart — no LIVE entries yet.
    vi.mocked(getLiveBrainSnapshot).mockReturnValue(healthySnapshot('SHADOW') as never);
    expect(liveEntriesAllowed()).toBe(false);
    expect(getRuntimeModeState().authoritative_mode).toBe('SHADOW');

    // Feed restored with authoritative LIVE — entries armed again.
    vi.mocked(getLiveBrainSnapshot).mockReturnValue(healthySnapshot('LIVE') as never);
    expect(liveEntriesAllowed()).toBe(true);
    expect(getRuntimeModeState().authoritative_mode).toBe('LIVE');
  });
});
