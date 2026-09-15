import { describe, it, expect } from 'vitest';
import { apiFetch, useApi } from '../src/hooks/useApi';
import { clientFetch, getClientToken, setClientToken } from '../src/hooks/useClientApi';
import { useWebSocket, subscribeWs } from '../src/hooks/useWebSocket';
import { useClientWebSocket } from '../src/hooks/useClientWebSocket';

describe('VS Control Panel + Client surface', () => {
  it('exposes control API helpers', () => {
    expect(typeof apiFetch).toBe('function');
    expect(typeof useApi).toBe('function');
    expect(typeof useWebSocket).toBe('function');
    expect(typeof subscribeWs).toBe('function');
  });

  it('exposes client API helpers', () => {
    expect(typeof clientFetch).toBe('function');
    expect(typeof getClientToken).toBe('function');
    expect(typeof setClientToken).toBe('function');
    expect(typeof useClientWebSocket).toBe('function');
  });
});
