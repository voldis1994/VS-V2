import { FastifyInstance } from 'fastify';
import { pool, healthCheck } from '../db/pool.js';
import { TelemetryBroadcaster } from '../ws/telemetry.js';
import { getBrainFeedStatus } from '../services/liveBrainFeed.js';
import { getPipelineBridgeStatus } from '../services/pipelineBridge.js';
import { marketCoreAuthoritative } from '../config/environment.js';
import {
  getRuntimeModeState,
  switchRuntimeMode,
  RUNTIME_MODES,
} from '../services/runtimeMode.js';

function liveEnabled(): boolean {
  const v = process.env.LIVE_TRADING_ENABLED;
  if (v === undefined || v === '') return false; // fail-closed
  return v === 'true' || v === '1';
}

export async function registerSystemRoutes(
  app: FastifyInstance,
  telemetry: TelemetryBroadcaster
): Promise<void> {
  app.get('/health', async () => ({ status: 'ok' }));

  app.get('/api/system/status', async () => {
    const dbOk = await healthCheck();
    let openPositions = 0;
    let todayExecutions = 0;
    let clientsActive = 0;
    let brokersLive = 0;
    let capitalMarkets = 0;
    let feedActive = 0;
    let feedUnhealthy = 0;
    let capitalSenders = 0;
    try {
      const [pos, execs, clients, brokers, markets] = await Promise.all([
        pool.query(`SELECT COUNT(*)::int AS n FROM positions WHERE status = 'OPEN'`),
        pool.query(
          `SELECT COUNT(*)::int AS n FROM executions WHERE executed_at::date = CURRENT_DATE`
        ),
        pool.query(`SELECT COUNT(*)::int AS n FROM clients WHERE enabled = true`),
        pool.query(
          `SELECT COUNT(*)::int AS n FROM broker_connections WHERE enabled = true AND environment = 'live'`
        ),
        pool.query(`SELECT COUNT(*)::int AS n FROM capital_markets`),
      ]);
      openPositions = pos.rows[0]?.n ?? 0;
      todayExecutions = execs.rows[0]?.n ?? 0;
      clientsActive = clients.rows[0]?.n ?? 0;
      brokersLive = brokers.rows[0]?.n ?? 0;
      capitalMarkets = markets.rows[0]?.n ?? 0;
    } catch {
      // tables may be mid-migrate
    }
    try {
      const { listDataSenders } = await import('../services/robotReader.js');
      const senders = await listDataSenders();
      capitalSenders = senders.filter((s) => s.kind === 'capital_com').length;
      feedActive = senders.filter((s) => s.status === 'LIVE').length;
      feedUnhealthy = senders.filter((s) => s.status === 'ERROR').length;
    } catch {
      /* robot reader optional on first boot */
    }

    return {
      market_core: (() => { const b = getBrainFeedStatus(); const p = getPipelineBridgeStatus(); if (b.connected || p.healthy) return 'HEALTHY'; if (b.last_error || p.last_error) return 'UNHEALTHY'; return 'UNKNOWN'; })(),
      execution: (() => { const b = getBrainFeedStatus(); if (!b.connected) return 'UNKNOWN'; return b.connected ? 'HEALTHY' : 'UNKNOWN'; })(),
      database: dbOk ? 'HEALTHY' : 'UNHEALTHY',
      postgres: dbOk ? 'ok' : 'down',
      redis: 'ok',
      control_api: 'HEALTHY',
      feeds: { active: feedActive, unhealthy: feedUnhealthy },
      clients: { active: clientsActive },
      brokers_live: brokersLive,
      live_brokers: brokersLive,
      capital_senders: capitalSenders,
      capital_markets: capitalMarkets,
      open_positions: openPositions,
      today_executions: todayExecutions,
      mode: getRuntimeModeState().mode,
      authoritative_mode: getRuntimeModeState().authoritative_mode,
      live_enabled: liveEnabled(),
      live_entries_allowed: getRuntimeModeState().live_entries_allowed,
      manage_open_positions: getRuntimeModeState().manage_open_positions,
      runtime_health: getRuntimeModeState().health,
      server_time: new Date().toISOString(),
      latency: telemetry.getLatestMetrics(),
      status: !dbOk ? 'DEGRADED' : (liveEnabled() && getRuntimeModeState().mode === 'LIVE' ? 'LIVE' : 'READY'),
      market_core_authoritative: marketCoreAuthoritative(),
      brain_feed: getBrainFeedStatus(),
      pipeline_bridge: getPipelineBridgeStatus(),
    };
  });

  app.get('/api/system/mode', async () => {
    const state = getRuntimeModeState();
    return {
      mode: state.mode,
      authoritative_mode: state.authoritative_mode,
      cpp_authoritative: state.cpp_authoritative,
      live_enabled: state.live_trading_enabled,
      live_entries_allowed: state.live_entries_allowed,
      manage_open_positions: state.manage_open_positions,
      health: state.health,
      last_switch: state.last_switch,
      allowed: [...RUNTIME_MODES],
    };
  });

  app.post('/api/system/mode', async (request, reply) => {
    const body = request.body as {
      mode?: string;
      confirm?: boolean;
      actor?: string;
      confirmed?: boolean;
    };
    const result = await switchRuntimeMode({
      mode: String(body.mode || ''),
      confirm: body.confirm === true || body.confirmed === true,
      actor: body.actor,
    });
    if (!result.ok) {
      return reply.code(400).send({
        error: result.error,
        mode: result.state.mode,
        health: result.health ?? result.state.health,
        state: result.state,
      });
    }
    return {
      mode: result.state.mode,
      previous: result.state.last_switch.from,
      live_enabled: result.state.live_trading_enabled,
      live_entries_allowed: result.state.live_entries_allowed,
      manage_open_positions: result.state.manage_open_positions,
      authoritative_mode: result.state.authoritative_mode,
      health: result.state.health,
      last_switch: result.state.last_switch,
    };
  });

  app.get('/api/system/runtime-mode', async () => getRuntimeModeState());

  app.post('/api/system/runtime-mode', async (request, reply) => {
    const body = request.body as {
      mode?: string;
      confirm?: boolean;
      actor?: string;
      confirmed?: boolean;
    };
    const result = await switchRuntimeMode({
      mode: String(body.mode || ''),
      confirm: body.confirm === true || body.confirmed === true,
      actor: body.actor,
    });
    if (!result.ok) {
      return reply.code(400).send({
        error: result.error,
        health: result.health ?? result.state.health,
        state: result.state,
      });
    }
    return result.state;
  });

  app.get('/api/system/metrics', async () => telemetry.getLatestMetrics());

  app.get('/api/system/events', async () => {
    const { rows } = await pool.query(
      'SELECT * FROM system_events ORDER BY created_at DESC LIMIT 100'
    );
    return rows;
  });
}
