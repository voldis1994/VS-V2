
export type OperatingMode = 'REPLAY' | 'PAPER' | 'DEMO' | 'LIVE';
export function operatingMode(): OperatingMode {
  const m = (process.env.OPERATING_MODE || 'PAPER').toUpperCase();
  if (m === 'REPLAY' || m === 'PAPER' || m === 'DEMO' || m === 'LIVE') return m;
  return 'PAPER';
}
export function liveTradingEnabled(): boolean {
  return (process.env.LIVE_TRADING_ENABLED || 'false').toLowerCase() === 'true';
}
