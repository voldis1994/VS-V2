import { beforeEach, describe, expect, it, vi } from 'vitest';

vi.mock('../db/pool.js', () => ({
  healthCheck: vi.fn(async () => true),
}));

vi.mock('./liveBrainFeed.js', () => ({
  getBrainFeedStatus: vi.fn(() => ({
    connected: false,
    model_id: null,
    model_version: null,
  })),
}));

vi.mock('../config/environment.js', () => ({
  marketCoreAuthoritative: vi.fn(() => true),
}));

vi.mock('./runtimeMode.js', () => ({
  liveEntriesAllowed: vi.fn(() => false),
  getRuntimeModeState: vi.fn(() => ({
    mode: 'PAPER',
    authoritative_mode: 'PAPER',
    cpp_authoritative: true,
    live_entries_allowed: false,
    live_trading_enabled: false,
    manage_open_positions: false,
    health: { ready: false, reasons: [] },
    last_switch: null,
  })),
}));

import { healthCheck } from '../db/pool.js';
import { liveEntriesAllowed, getRuntimeModeState } from './runtimeMode.js';
import { runPaperPreflight } from './paperPreflight.js';

describe('paperPreflight', () => {
  beforeEach(() => {
    process.env.OPERATING_MODE = 'PAPER';
    process.env.LIVE_TRADING_ENABLED = 'false';
    process.env.CAPITAL_API_KEY = 'k';
    process.env.CAPITAL_API_PASSWORD = 'p';
    process.env.CAPITAL_IDENTIFIER = 'id';
    process.env.CAPITAL_EPIC = 'GOLD';
    process.env.CAPITAL_BASE_URL = 'https://api-capital.backend-capital.com';
    vi.mocked(healthCheck).mockResolvedValue(true);
    vi.mocked(liveEntriesAllowed).mockReturnValue(false);
    vi.mocked(getRuntimeModeState).mockReturnValue({
      mode: 'PAPER',
      authoritative_mode: 'PAPER',
      cpp_authoritative: true,
      live_entries_allowed: false,
      live_trading_enabled: false,
      manage_open_positions: false,
      health: { ready: false, reasons: [] },
      last_switch: null,
    } as never);
  });

  it('passes fail-closed PAPER with LIVE market-data creds', async () => {
    const r = await runPaperPreflight();
    expect(r.ok).toBe(true);
    expect(r.data_ready).toBe(true);
    expect(r.broker_orders_forbidden).toBe(true);
    expect(r.live_trading_enabled).toBe(false);
    expect(r.live_entries_allowed).toBe(false);
    expect(r.operating_mode).toBe('PAPER');
  });

  it('fails when LIVE trading is armed', async () => {
    process.env.LIVE_TRADING_ENABLED = 'true';
    vi.mocked(getRuntimeModeState).mockReturnValue({
      mode: 'PAPER',
      authoritative_mode: 'PAPER',
      cpp_authoritative: true,
      live_entries_allowed: false,
      live_trading_enabled: true,
      manage_open_positions: false,
      health: { ready: false, reasons: [] },
      last_switch: null,
    } as never);
    const r = await runPaperPreflight();
    expect(r.ok).toBe(false);
    expect(r.reasons).toContain('live_trading_enabled');
  });

  it('fails when live entries are allowed', async () => {
    vi.mocked(liveEntriesAllowed).mockReturnValue(true);
    vi.mocked(getRuntimeModeState).mockReturnValue({
      mode: 'PAPER',
      authoritative_mode: 'PAPER',
      cpp_authoritative: true,
      live_entries_allowed: true,
      live_trading_enabled: false,
      manage_open_positions: false,
      health: { ready: false, reasons: [] },
      last_switch: null,
    } as never);
    const r = await runPaperPreflight();
    expect(r.ok).toBe(false);
    expect(r.reasons).toContain('live_entries_allowed');
    expect(r.broker_orders_forbidden).toBe(false);
  });

  it('fails when OPERATING_MODE is not PAPER', async () => {
    process.env.OPERATING_MODE = 'SHADOW';
    vi.mocked(getRuntimeModeState).mockReturnValue({
      mode: 'SHADOW',
      authoritative_mode: 'SHADOW',
      cpp_authoritative: true,
      live_entries_allowed: false,
      live_trading_enabled: false,
      manage_open_positions: true,
      health: { ready: false, reasons: [] },
      last_switch: null,
    } as never);
    const r = await runPaperPreflight();
    expect(r.ok).toBe(false);
    expect(r.reasons).toContain('operating_mode_not_paper');
  });

  it('ok without Capital but data_ready false', async () => {
    delete process.env.CAPITAL_API_KEY;
    const r = await runPaperPreflight();
    expect(r.ok).toBe(true);
    expect(r.data_ready).toBe(false);
    expect(r.reasons).toContain('capital_credentials_incomplete');
  });

  it('rejects demo Capital base URL for LIVE market data', async () => {
    process.env.CAPITAL_BASE_URL = 'https://demo-api-capital.backend-capital.com';
    const r = await runPaperPreflight();
    expect(r.ok).toBe(true);
    expect(r.data_ready).toBe(false);
    expect(r.reasons).toContain('capital_base_url_demo');
  });
});
