import { FastifyInstance } from 'fastify';
import { logAudit } from '../services/audit.js';

/** Fail-closed: LIVE trading requires explicit LIVE_TRADING_ENABLED=true. */
function liveEnabled(): boolean {
  const v = process.env.LIVE_TRADING_ENABLED;
  if (v === undefined || v === '') return false;
  return v === 'true' || v === '1';
}

export async function registerSettingsRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/settings', async () => ({
    operating_mode: process.env.OPERATING_MODE || 'PAPER',
    live_trading_enabled: liveEnabled(),
    operating_modes: ['PAPER', 'SHADOW', 'LIVE'],
    primary_horizon_ms: 10000,
    entry_ttl_ms: 2000,
    log_level: process.env.LOG_LEVEL || 'info',
    runtime_mode_path: '/api/system/runtime-mode',
  }));

  /**
   * Settings may update non-arming prefs only (log level).
   * OPERATING_MODE / LIVE_TRADING_ENABLED must go through POST /api/system/runtime-mode
   * (confirm + health gates) — never via this PUT.
   */
  app.put('/api/settings', async (request, reply) => {
    const body = request.body as {
      log_level?: string;
      live_trading_enabled?: boolean;
      operating_mode?: string;
    };

    if (typeof body.live_trading_enabled === 'boolean' || typeof body.operating_mode === 'string') {
      return reply.code(400).send({
        error: 'Use POST /api/system/runtime-mode to change operating mode or arm LIVE',
        message:
          'PUT /api/settings cannot set OPERATING_MODE or LIVE_TRADING_ENABLED. ' +
          'Use POST /api/system/runtime-mode with confirm=true (fail-closed health gates).',
        runtime_mode_path: '/api/system/runtime-mode',
      });
    }

    if (body.log_level) {
      const prev = process.env.LOG_LEVEL || 'info';
      process.env.LOG_LEVEL = body.log_level;
      await logAudit('admin', 'log_level_updated', 'settings', 'LOG_LEVEL', { log_level: prev }, {
        log_level: body.log_level,
      });
    }

    return {
      success: true,
      operating_mode: process.env.OPERATING_MODE || 'PAPER',
      live_trading_enabled: liveEnabled(),
      log_level: process.env.LOG_LEVEL || 'info',
    };
  });
}
