
import { listRegimeSnapshots } from './regimes.js';

export async function listBrainSnapshots() {
  return listRegimeSnapshots().map((row, i) => ({
    instrument_id: i + 1,
    epic: row.epic,
    bias: row.current.includes('UP') ? 'BULLISH' : row.current.includes('DOWN') ? 'BEARISH' : 'NEUTRAL',
    structure: row.confidence * 0.6,
    momentum: row.confidence * 0.4,
    pressure: 0,
    composite: row.confidence,
    bar_count: row.bar_count,
  }));
}
