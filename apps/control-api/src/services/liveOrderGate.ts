/**
 * Fail-closed gate for real broker order placement.
 * Manual Trading desk orders must use the same arming rules as intent fan-out.
 */
import { liveEntriesAllowed } from './runtimeMode.js';

export type LiveOrderGateResult =
  | { allowed: true }
  | { allowed: false; statusCode: 403; error: string; message: string };

/** Refuse Capital/broker creates unless LIVE entries are explicitly armed. */
export function assertLiveOrdersAllowed(): LiveOrderGateResult {
  if (liveEntriesAllowed()) return { allowed: true };
  const mode = (process.env.OPERATING_MODE || 'PAPER').toUpperCase();
  const live = process.env.LIVE_TRADING_ENABLED || 'false';
  const msg =
    `Broker orders forbidden (fail-closed): OPERATING_MODE=${mode}, LIVE_TRADING_ENABLED=${live}. ` +
    'Arm LIVE via POST /api/system/runtime-mode with confirm=true (not PUT /api/settings).';
  return { allowed: false, statusCode: 403, error: msg, message: msg };
}
