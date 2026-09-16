import { describe, expect, it } from 'vitest';
import {
  capitalComBaseUrl,
  encryptCapitalPassword,
  fetchAllCapitalMarkets,
  testCapitalComSession,
  type CapitalSession,
} from './capitalCom.js';
import { generateKeyPairSync } from 'crypto';

describe('capitalComBaseUrl', () => {
  it('uses live host for live', () => {
    expect(capitalComBaseUrl('live')).toBe('https://api-capital.backend-capital.com');
  });

  it('defaults to demo host otherwise', () => {
    expect(capitalComBaseUrl('demo')).toBe('https://demo-api-capital.backend-capital.com');
    expect(capitalComBaseUrl('other')).toBe('https://demo-api-capital.backend-capital.com');
  });
});

describe('testCapitalComSession validation', () => {
  it('rejects email used as API key without calling network', async () => {
    const result = await testCapitalComSession({
      environment: 'live',
      apiKey: 'user@inbox.lv',
      identifier: 'user@inbox.lv',
      password: 'secret',
    });
    expect(result.ok).toBe(false);
    expect(result.detail.toLowerCase()).toContain('email');
  });

  it('rejects 6-digit OTP pasted as API password', async () => {
    const result = await testCapitalComSession({
      environment: 'live',
      apiKey: 'real-api-key-string',
      identifier: 'user@inbox.lv',
      password: '123456',
    });
    expect(result.ok).toBe(false);
    expect(result.detail.toLowerCase()).toContain('2fa');
  });
});

describe('encryptCapitalPassword', () => {
  it('produces base64 ciphertext with RSA public key', () => {
    const { publicKey } = generateKeyPairSync('rsa', { modulusLength: 2048 });
    const der = publicKey.export({ type: 'spki', format: 'der' }).toString('base64');
    const out = encryptCapitalPassword(der, 1710000000, 'api-password');
    expect(out.length).toBeGreaterThan(20);
    expect(() => Buffer.from(out, 'base64')).not.toThrow();
  });
});

describe('fetchAllCapitalMarkets', () => {
  it('uses GET /markets (all) and seeds epics into catalog', async () => {
    const calls: string[] = [];
    const session: CapitalSession = {
      base: 'https://demo-api-capital.backend-capital.com',
      apiKey: 'k',
      cst: 'c',
      securityToken: 's',
      close: async () => undefined,
      get: async (path: string) => {
        calls.push(path);
        if (path === '/api/v1/markets') {
          return {
            ok: true,
            status: 200,
            text: '',
            json: {
              markets: [
                {
                  epic: 'EURUSD',
                  instrumentName: 'EUR/USD',
                  instrumentType: 'CURRENCIES',
                },
                {
                  epic: 'GOLD',
                  instrumentName: 'Gold',
                  instrumentType: 'COMMODITIES',
                },
              ],
            },
          };
        }
        if (path.startsWith('/api/v1/markets?epics=')) {
          return {
            ok: true,
            status: 200,
            text: '',
            json: {
              markets: [
                {
                  epic: 'BTCUSD',
                  instrumentName: 'Bitcoin',
                  instrumentType: 'CRYPTOCURRENCIES',
                },
              ],
            },
          };
        }
        return { ok: true, status: 200, text: '', json: { markets: [] } };
      },
      post: async () => ({ ok: true, status: 200, text: '', json: {} }),
      put: async () => ({ ok: true, status: 200, text: '', json: {} }),
      del: async () => ({ ok: true, status: 200, text: '', json: {} }),
    };

    const { markets, diagnostics } = await fetchAllCapitalMarkets(session, { mode: 'quick' });
    expect(calls.some((c) => c === '/api/v1/markets')).toBe(true);
    expect(calls.some((c) => c.startsWith('/api/v1/markets?epics='))).toBe(true);
    expect(markets.map((m) => m.epic).sort()).toEqual(['BTCUSD', 'EURUSD', 'GOLD']);
    expect(diagnostics.sources[0]?.source).toContain('/markets (all)');
    expect(markets.find((m) => m.epic === 'EURUSD')?.category).toBe('fx');
  });

  it('still seeds from epics when unfiltered list fails', async () => {
    const session: CapitalSession = {
      base: 'https://demo-api-capital.backend-capital.com',
      apiKey: 'k',
      cst: 'c',
      securityToken: 's',
      close: async () => undefined,
      get: async (path: string) => {
        if (path === '/api/v1/markets') {
          return {
            ok: false,
            status: 403,
            text: 'denied',
            json: { errorCode: 'error.forbidden' },
          };
        }
        if (path.startsWith('/api/v1/markets?epics=')) {
          return {
            ok: true,
            status: 200,
            text: '',
            json: {
              markets: [{ epic: 'SILVER', instrumentName: 'Silver', instrumentType: 'COMMODITIES' }],
            },
          };
        }
        if (path.includes('searchTerm=')) {
          return { ok: true, status: 200, text: '', json: { markets: [] } };
        }
        return { ok: true, status: 200, text: '', json: { markets: [] } };
      },
      post: async () => ({ ok: true, status: 200, text: '', json: {} }),
      put: async () => ({ ok: true, status: 200, text: '', json: {} }),
      del: async () => ({ ok: true, status: 200, text: '', json: {} }),
    };

    const { markets, diagnostics } = await fetchAllCapitalMarkets(session, { mode: 'quick' });
    expect(markets.some((m) => m.epic === 'SILVER')).toBe(true);
    expect(diagnostics.sources.find((s) => s.source.includes('(all)'))?.ok).toBe(false);
  });
});
