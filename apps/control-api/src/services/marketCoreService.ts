
import { listRegimeSnapshots } from './regimes.js';

export async function marketCoreStatus() {
  const instruments = listRegimeSnapshots();
  return {
    status: 'ONLINE',
    pipeline: 'normalize→quality→fusion→candles→brain→decision→risk',
    instruments: instruments.length,
    timestamp: new Date().toISOString(),
  };
}
