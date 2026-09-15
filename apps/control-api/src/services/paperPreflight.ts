/**
 * PAPER deployment preflight — fail-closed readiness without arming LIVE or sending orders.
 */
import { healthCheck } from '../db/pool.js';
import { liveEntriesAllowed, getRuntimeModeState } from './runtimeMode.js';
import { getBrainFeedStatus } from './liveBrainFeed.js';
import { marketCoreAuthoritative } from '../config/environment.js';

export type PaperPreflightResult = {
  ok: boolean;
  data_ready: boolean;
  operating_mode: string;
  live_trading_enabled: boolean;
  live_entries_allowed: boolean;
  broker_orders_forbidden: boolean;
  checks: Record<string, { ok: boolean; detail?: string }>;
  reasons: string[];
};

function liveTradingFlag(): boolean {
  const v = process.env.LIVE_TRADING_ENABLED;
  if (v === undefined || v === '') return false;
  return v === 'true' || v === '1';
}

export async function runPaperPreflight(): Promise<PaperPreflightResult> {
  const reasons: string[] = [];
  const checks: PaperPreflightResult['checks'] = {};

  const mode = (process.env.OPERATING_MODE || 'PAPER').toUpperCase();
  const modeOk = mode === 'PAPER';
  checks.operating_mode_paper = {
    ok: modeOk,
    detail: `OPERATING_MODE=${mode}`,
  };
  if (!modeOk) reasons.push('operating_mode_not_paper');

  const liveEnabled = liveTradingFlag();
  checks.live_trading_disabled = {
    ok: !liveEnabled,
    detail: `LIVE_TRADING_ENABLED=${process.env.LIVE_TRADING_ENABLED ?? 'false'}`,
  };
  if (liveEnabled) reasons.push('live_trading_enabled');

  const entries = liveEntriesAllowed();
  checks.live_entries_blocked = {
    ok: !entries,
    detail: entries ? 'live_entries_allowed=true' : 'live_entries_allowed=false',
  };
  if (entries) reasons.push('live_entries_allowed');

  // Manual + automated Capital creates share liveEntriesAllowed() (trading route + intent fan-out).
  checks.broker_order_routes_gated = {
    ok: !entries,
    detail: !entries
      ? 'POST /api/trading/accounts/:id/orders requires liveEntriesAllowed()'
      : 'live entries armed — broker order routes may place Capital orders',
  };

  const dbOk = await healthCheck();
  checks.database = { ok: dbOk, detail: dbOk ? 'HEALTHY' : 'UNHEALTHY' };
  if (!dbOk) reasons.push('database_unhealthy');

  const capitalKey = Boolean(process.env.CAPITAL_API_KEY);
  const capitalPw = Boolean(process.env.CAPITAL_API_PASSWORD);
  const capitalId = Boolean(process.env.CAPITAL_IDENTIFIER);
  const capitalEpic = Boolean(process.env.CAPITAL_EPIC);
  const capitalReady = capitalKey && capitalPw && capitalId && capitalEpic;
  const baseUrl = process.env.CAPITAL_BASE_URL || 'https://api-capital.backend-capital.com';
  const baseLooksDemo = /demo/i.test(baseUrl);
  checks.capital_live_market_data_creds = {
    ok: capitalReady,
    detail: capitalReady
      ? `epic=${process.env.CAPITAL_EPIC}; base=${baseUrl}`
      : 'missing CAPITAL_API_KEY/PASSWORD/IDENTIFIER/EPIC',
  };
  checks.capital_base_url_not_demo = {
    ok: !baseLooksDemo,
    detail: baseUrl,
  };
  if (baseLooksDemo) reasons.push('capital_base_url_demo');
  if (!capitalReady) reasons.push('capital_credentials_incomplete');

  const brain = getBrainFeedStatus();
  checks.market_core_bridge = {
    ok: true,
    detail: brain.connected
      ? `brain_feed connected authoritative=${marketCoreAuthoritative()}`
      : `brain_feed disconnected (acceptable at boot) authoritative=${marketCoreAuthoritative()}`,
  };

  const state = getRuntimeModeState();
  checks.runtime_mode_state = {
    ok: state.mode === 'PAPER' && !state.live_trading_enabled && !state.live_entries_allowed,
    detail: `mode=${state.mode} live_trading=${state.live_trading_enabled} entries=${state.live_entries_allowed}`,
  };
  if (!checks.runtime_mode_state.ok) reasons.push('runtime_mode_not_paper_fail_closed');

  // Trading fail-closed gates are mandatory for ok.
  const tradingSafe =
    checks.operating_mode_paper.ok &&
    checks.live_trading_disabled.ok &&
    checks.live_entries_blocked.ok &&
    checks.database.ok &&
    checks.runtime_mode_state.ok;

  const dataReady = checks.capital_live_market_data_creds.ok && checks.capital_base_url_not_demo.ok;

  return {
    ok: tradingSafe,
    data_ready: dataReady,
    operating_mode: mode,
    live_trading_enabled: liveEnabled,
    live_entries_allowed: entries,
    broker_orders_forbidden: !entries,
    checks,
    reasons: tradingSafe
      ? reasons.filter((r) => r.startsWith('capital_'))
      : reasons,
  };
}
