import Fastify from 'fastify';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

vi.mock('../services/audit.js', () => ({
  logAudit: vi.fn(async () => undefined),
}));

import { registerSettingsRoutes } from './settings.js';

describe('settings routes fail-closed arming', () => {
  beforeEach(() => {
    process.env.OPERATING_MODE = 'PAPER';
    process.env.LIVE_TRADING_ENABLED = 'false';
    process.env.LOG_LEVEL = 'info';
  });

  afterEach(() => {
    process.env.OPERATING_MODE = 'PAPER';
    process.env.LIVE_TRADING_ENABLED = 'false';
  });

  async function app() {
    const f = Fastify();
    await registerSettingsRoutes(f);
    await f.ready();
    return f;
  }

  it('rejects LIVE arming via PUT /api/settings', async () => {
    const f = await app();
    const res = await f.inject({
      method: 'PUT',
      url: '/api/settings',
      payload: { operating_mode: 'LIVE', live_trading_enabled: true },
    });
    expect(res.statusCode).toBe(400);
    expect(res.json().message).toMatch(/runtime-mode/);
    expect(process.env.OPERATING_MODE).toBe('PAPER');
    expect(process.env.LIVE_TRADING_ENABLED).toBe('false');
    await f.close();
  });

  it('allows log_level only', async () => {
    const f = await app();
    const res = await f.inject({
      method: 'PUT',
      url: '/api/settings',
      payload: { log_level: 'debug' },
    });
    expect(res.statusCode).toBe(200);
    expect(process.env.LOG_LEVEL).toBe('debug');
    expect(process.env.OPERATING_MODE).toBe('PAPER');
    await f.close();
  });
});
