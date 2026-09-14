
export type OperatingMode = 'PAPER' | 'SHADOW' | 'LIVE';

export function operatingMode(): OperatingMode {
  const m = (process.env.OPERATING_MODE || 'PAPER').toUpperCase();
  if (m === 'PAPER' || m === 'SHADOW' || m === 'LIVE') return m;
  // Legacy aliases
  if (m === 'REPLAY') return 'PAPER';
  if (m === 'DEMO') return 'SHADOW';
  return 'PAPER';
}

export function liveTradingEnabled(): boolean {
  return (process.env.LIVE_TRADING_ENABLED || 'false').toLowerCase() === 'true';
}

/** When true, C++ market-core owns entry — TS robotDesk must not run parallel entry brain. */
export function marketCoreAuthoritative(): boolean {
  const v = (process.env.MARKET_CORE_BRIDGE || process.env.VS_V2_CPP_AUTHORITATIVE || 'true').toLowerCase();
  return v === '1' || v === 'true' || v === 'yes';
}
