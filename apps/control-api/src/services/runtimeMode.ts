/**
 * VS-V2 runtime mode switch — PAPER / SHADOW / LIVE.
 * C++ market-core is authoritative for operating_mode when a brain snapshot is live.
 * Control API gates operator switches with health checks, confirmation, and audit.
 */

import { logAudit } from './audit.js';
import { getBrainFeedStatus, getLiveBrainSnapshot } from './liveBrainFeed.js';
import { getPipelineBridgeStatus } from './pipelineBridge.js';
import { marketCoreAuthoritative } from '../config/environment.js';

export const RUNTIME_MODES = ['PAPER', 'SHADOW', 'LIVE'] as const;
export type RuntimeModeName = (typeof RUNTIME_MODES)[number];

export type HealthGate = {
  ok: boolean;
  broker: 'OK' | 'FAIL' | 'UNKNOWN';
  data: 'OK' | 'FAIL' | 'UNKNOWN';
  model: 'OK' | 'FAIL' | 'UNKNOWN';
  risk: 'OK' | 'FAIL' | 'UNKNOWN';
  reasons: string[];
};

export type RuntimeModeState = {
  mode: RuntimeModeName;
  /** Authoritative mode from C++ brain feed when connected; else control-api requested mode. */
  authoritative_mode: RuntimeModeName | 'UNKNOWN';
  cpp_authoritative: boolean;
  live_entries_allowed: boolean;
  live_trading_enabled: boolean;
  manage_open_positions: boolean;
  health: HealthGate;
  last_switch: {
    from: string | null;
    to: string | null;
    actor: string | null;
    at: string | null;
    confirmed: boolean | null;
  };
};

export type SwitchResult =
  | { ok: true; state: RuntimeModeState }
  | { ok: false; error: string; state: RuntimeModeState; health?: HealthGate };

let lastSwitch: RuntimeModeState['last_switch'] = {
  from: null,
  to: null,
  actor: null,
  at: null,
  confirmed: null,
};

function normalizeMode(raw: string | null | undefined): RuntimeModeName | null {
  const m = String(raw || '').trim().toUpperCase();
  if (m === 'PAPER' || m === 'SHADOW' || m === 'LIVE') return m;
  // Legacy aliases — map into the three operator modes.
  if (m === 'REPLAY') return 'PAPER';
  if (m === 'DEMO') return 'SHADOW';
  return null;
}

function requestedMode(): RuntimeModeName {
  return normalizeMode(process.env.OPERATING_MODE) ?? 'PAPER';
}

/** New LIVE entries only when explicitly in LIVE and armed. */
export function liveEntriesAllowed(): boolean {
  // Control-api arming gate first (fail-closed).
  if (requestedMode() !== 'LIVE') return false;
  if ((process.env.LIVE_TRADING_ENABLED || 'false').toLowerCase() !== 'true') return false;
  // When C++ market-core is authoritative: LIVE entries require a connected brain feed
  // whose operating_mode is explicitly LIVE. Disconnect / missing snapshot / UNKNOWN → false.
  if (marketCoreAuthoritative()) {
    const snap = getLiveBrainSnapshot();
    const feed = getBrainFeedStatus();
    if (!feed.connected || !snap?.operating_mode) return false;
    return normalizeMode(snap.operating_mode) === 'LIVE';
  }
  // Non-C++ deployments: confirmed control-api LIVE arming is sufficient.
  return true;
}

/** Open LIVE positions may continue manage/exit in LIVE and after LIVE→SHADOW demotion. */
export function manageOpenPositionsAllowed(): boolean {
  const mode = authoritativeRuntimeMode();
  if (mode === 'UNKNOWN') {
    const req = requestedMode();
    return req === 'LIVE' || req === 'SHADOW';
  }
  return mode === 'LIVE' || mode === 'SHADOW';
}

export function authoritativeRuntimeMode(): RuntimeModeName | 'UNKNOWN' {
  if (marketCoreAuthoritative()) {
    const snap = getLiveBrainSnapshot();
    const feed = getBrainFeedStatus();
    if (feed.connected && snap?.operating_mode) {
      return normalizeMode(snap.operating_mode) ?? 'UNKNOWN';
    }
    // C++ owns mode — missing/disconnected feed is UNKNOWN (never invent LIVE).
    return 'UNKNOWN';
  }
  return requestedMode();
}

/**
 * Fail-closed LIVE readiness: broker, data, model, risk must all be ready.
 * Missing/unknown signals are failures (never invent OK).
 */
export function evaluateLiveReadiness(): HealthGate {
  const reasons: string[] = [];
  const brain = getBrainFeedStatus();
  const bridge = getPipelineBridgeStatus();
  const snap = getLiveBrainSnapshot();

  let broker: HealthGate['broker'] = 'UNKNOWN';
  let data: HealthGate['data'] = 'UNKNOWN';
  let model: HealthGate['model'] = 'UNKNOWN';
  let risk: HealthGate['risk'] = 'UNKNOWN';

  const execHealth = String(snap?.health?.execution ?? '').toUpperCase();
  const feedsHealth = String(snap?.health?.feeds ?? '').toUpperCase();
  const dataHealth = String(snap?.health?.data ?? '').toUpperCase();
  const coreHealth = String(snap?.health?.market_core ?? '').toUpperCase();

  if (bridge.healthy || execHealth === 'HEALTHY') broker = 'OK';
  else if (bridge.last_error || execHealth === 'UNHEALTHY' || execHealth === 'DISCONNECTED') {
    broker = 'FAIL';
    reasons.push('broker_unhealthy');
  } else {
    reasons.push('broker_unknown');
  }

  if (
    brain.connected &&
    (feedsHealth === 'HEALTHY' || dataHealth === 'HEALTHY' || coreHealth === 'HEALTHY')
  ) {
    data = 'OK';
  } else if (!brain.connected || feedsHealth === 'UNHEALTHY' || dataHealth === 'UNHEALTHY') {
    data = 'FAIL';
    reasons.push(brain.connected ? 'data_unhealthy' : 'data_feed_disconnected');
  } else {
    reasons.push('data_unknown');
  }

  const modelId = snap?.model_id ?? brain.model_id;
  const modelVersion = snap?.model_version ?? brain.model_version;
  if (modelId && modelVersion && modelId !== 'default') model = 'OK';
  else if (modelId && modelVersion) {
    // default@0.0.0 is not a production load — fail-closed for LIVE
    model = 'FAIL';
    reasons.push('production_model_not_loaded');
  } else {
    reasons.push('model_unknown');
  }

  const instruments = snap?.instruments ?? [];
  const riskReady = instruments.some(
    (i) => i.has_risk && i.risk && typeof i.risk.approved === 'boolean',
  );
  const riskVetoHard = instruments.some(
    (i) =>
      i.has_risk &&
      i.risk &&
      i.risk.approved === false &&
      (i.risk.reason_codes || []).some((c) => /BROKER|EQUITY|FATAL|UNHEALTHY/i.test(c)),
  );
  if (riskVetoHard) {
    risk = 'FAIL';
    reasons.push('risk_fail_closed');
  } else if (riskReady || (brain.connected && instruments.length > 0)) {
    risk = 'OK';
  } else {
    reasons.push('risk_unknown');
  }

  const ok = broker === 'OK' && data === 'OK' && model === 'OK' && risk === 'OK';
  return { ok, broker, data, model, risk, reasons };
}

export function getRuntimeModeState(): RuntimeModeState {
  const mode = requestedMode();
  const authoritative = authoritativeRuntimeMode();
  const health = evaluateLiveReadiness();
  const liveEnabled = (process.env.LIVE_TRADING_ENABLED || 'false').toLowerCase() === 'true';
  return {
    mode,
    authoritative_mode: authoritative,
    cpp_authoritative: marketCoreAuthoritative(),
    live_entries_allowed: liveEntriesAllowed(),
    live_trading_enabled: liveEnabled,
    manage_open_positions: manageOpenPositionsAllowed(),
    health,
    last_switch: { ...lastSwitch },
  };
}

/**
 * Operator mode switch.
 * LIVE requires confirm=true and full health readiness (fail-closed).
 * LIVE→SHADOW blocks new LIVE entries immediately; manage/exit remains allowed.
 * SHADOW→LIVE arms real execution only when healthy + confirmed.
 */
export async function switchRuntimeMode(input: {
  mode: string;
  confirm?: boolean;
  actor?: string;
}): Promise<SwitchResult> {
  const target = normalizeMode(input.mode);
  if (!target) {
    return {
      ok: false,
      error: `Invalid mode. Use: ${RUNTIME_MODES.join(', ')}`,
      state: getRuntimeModeState(),
    };
  }

  const prev = requestedMode();
  const actor = (input.actor || 'admin').trim() || 'admin';
  const confirmed = input.confirm === true;

  if (target === 'LIVE') {
    if (!confirmed) {
      return {
        ok: false,
        error: 'LIVE requires explicit confirmation (confirm=true)',
        state: getRuntimeModeState(),
      };
    }
    const health = evaluateLiveReadiness();
    if (!health.ok) {
      return {
        ok: false,
        error: `LIVE blocked (fail-closed): ${health.reasons.join(', ') || 'not_ready'}`,
        state: getRuntimeModeState(),
        health,
      };
    }
    process.env.OPERATING_MODE = 'LIVE';
    process.env.LIVE_TRADING_ENABLED = 'true';
  } else if (target === 'SHADOW') {
    // Demote or park in SHADOW — never leave LIVE entries armed.
    process.env.OPERATING_MODE = 'SHADOW';
    process.env.LIVE_TRADING_ENABLED = 'false';
  } else {
    process.env.OPERATING_MODE = 'PAPER';
    process.env.LIVE_TRADING_ENABLED = 'false';
  }

  lastSwitch = {
    from: prev,
    to: target,
    actor,
    at: new Date().toISOString(),
    confirmed: target === 'LIVE' ? true : confirmed || null,
  };

  await logAudit(actor, 'runtime_mode_switch', 'system', 'OPERATING_MODE', { mode: prev }, {
    mode: target,
    live_trading_enabled: process.env.LIVE_TRADING_ENABLED === 'true',
    live_entries_allowed: target === 'LIVE',
    manage_open_positions: target === 'LIVE' || target === 'SHADOW',
    confirmed: lastSwitch.confirmed,
    at: lastSwitch.at,
  });

  return { ok: true, state: getRuntimeModeState() };
}

/** Test helper — reset in-memory switch metadata. */
export function resetRuntimeModeForTests(): void {
  lastSwitch = { from: null, to: null, actor: null, at: null, confirmed: null };
  process.env.OPERATING_MODE = 'PAPER';
  process.env.LIVE_TRADING_ENABLED = 'false';
}
