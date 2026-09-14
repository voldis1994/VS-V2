import { describe, it, expect } from 'vitest';
import { useMarketStore } from '../src/state/marketStore';
import { useBrainStore } from '../src/state/brainStore';

describe('VS-V2 dashboard stores', () => {
  it('exposes market store API', () => {
    const state = useMarketStore.getState();
    expect(state).toBeTruthy();
    expect(typeof state).toBe('object');
  });

  it('exposes brain store API', () => {
    const state = useBrainStore.getState();
    expect(state).toBeTruthy();
  });
});

import { useLiveBrainStore } from '../src/state/liveBrainStore';

describe('live brain terminal store', () => {
  it('exposes live brain store API', () => {
    const state = useLiveBrainStore.getState();
    expect(typeof state.fetchLive).toBe('function');
    expect(typeof state.applySnapshot).toBe('function');
    expect(state.status?.invents_decisions ?? false).toBe(false);
  });
});
