import { beforeEach, describe, expect, it, vi } from 'vitest';

vi.mock('./runtimeMode.js', () => ({
  liveEntriesAllowed: vi.fn(() => false),
}));

import { liveEntriesAllowed } from './runtimeMode.js';
import { assertLiveOrdersAllowed } from './liveOrderGate.js';

describe('assertLiveOrdersAllowed', () => {
  beforeEach(() => {
    process.env.OPERATING_MODE = 'PAPER';
    process.env.LIVE_TRADING_ENABLED = 'false';
    vi.mocked(liveEntriesAllowed).mockReturnValue(false);
  });

  it('refuses PAPER / unarmed', () => {
    const r = assertLiveOrdersAllowed();
    expect(r.allowed).toBe(false);
    if (!r.allowed) {
      expect(r.statusCode).toBe(403);
      expect(r.message).toMatch(/Broker orders forbidden/);
      expect(r.message).toMatch(/runtime-mode/);
    }
  });

  it('allows when liveEntriesAllowed', () => {
    vi.mocked(liveEntriesAllowed).mockReturnValue(true);
    process.env.OPERATING_MODE = 'LIVE';
    process.env.LIVE_TRADING_ENABLED = 'true';
    expect(assertLiveOrdersAllowed()).toEqual({ allowed: true });
  });
});
