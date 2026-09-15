import { beforeEach, describe, expect, it, vi } from 'vitest';
import { authMiddleware, isPublicUnauthedPath } from './auth.js';

function mockReply() {
  const reply: any = {
    code: vi.fn((c: number) => {
      reply.statusCode = c;
      return reply;
    }),
    send: vi.fn((body: unknown) => {
      reply.body = body;
      return reply;
    }),
    statusCode: 200,
    body: null,
  };
  return reply;
}

describe('authMiddleware', () => {
  beforeEach(() => {
    delete process.env.API_ADMIN_TOKEN;
    delete process.env.ALLOW_INSECURE_ADMIN;
    delete process.env.NODE_ENV;
  });

  it('exposes preflight as public', () => {
    expect(isPublicUnauthedPath('GET', '/api/system/preflight')).toBe(true);
  });

  it('refuses CHANGE_ME_ADMIN_TOKEN without ALLOW_INSECURE_ADMIN', async () => {
    process.env.API_ADMIN_TOKEN = 'CHANGE_ME_ADMIN_TOKEN';
    const reply = mockReply();
    await authMiddleware({ method: 'GET', url: '/api/settings', headers: {} } as any, reply);
    expect(reply.code).toHaveBeenCalledWith(401);
  });

  it('allows CHANGE_ME_ADMIN_TOKEN when ALLOW_INSECURE_ADMIN=true', async () => {
    process.env.API_ADMIN_TOKEN = 'CHANGE_ME_ADMIN_TOKEN';
    process.env.ALLOW_INSECURE_ADMIN = 'true';
    const reply = mockReply();
    await authMiddleware({ method: 'GET', url: '/api/settings', headers: {} } as any, reply);
    expect(reply.code).not.toHaveBeenCalled();
  });

  it('accepts matching admin token', async () => {
    process.env.API_ADMIN_TOKEN = 'secret-token';
    const reply = mockReply();
    await authMiddleware(
      { method: 'GET', url: '/api/settings', headers: { 'x-admin-token': 'secret-token' } } as any,
      reply,
    );
    expect(reply.code).not.toHaveBeenCalled();
  });
});
