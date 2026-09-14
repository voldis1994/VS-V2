/**
 * Legacy brainService — DO NOT invent trading scores from regimes.
 * Prefer liveBrainFeed (authoritative market-core snapshots).
 */
import { getLiveBrainSnapshot } from './liveBrainFeed.js';

export async function listBrainSnapshots() {
  const live = getLiveBrainSnapshot();
  if (!live) return [];
  return live.instruments.map((inst) => ({
    instrument_id: inst.instrument_id,
    epic: inst.epic,
    symbol: inst.symbol,
    decision_action: inst.decision_action,
    expected_value: inst.decision.expected_value,
    probability: inst.decision.probability,
    mid: inst.quote.mid,
    has_structure_authority: inst.has_structure_authority,
    has_micro_authority: inst.has_micro_authority,
    source: 'market-core',
  }));
}
