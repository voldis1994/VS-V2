import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {
  resolveClientPublicUrl,
  setClientPublicUrl,
  getClientWebPublicState,
  loadClientPublicUrlFromDisk,
} from './clientPublicUrl.js';

describe('clientPublicUrl', () => {
  const prev = { ...process.env };
  let tmp: string;
  let cwd: string;

  beforeEach(() => {
    tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'vs-client-url-'));
    cwd = process.cwd();
    process.chdir(tmp);
    delete process.env.CLIENT_PUBLIC_URL;
    delete process.env.CLIENT_CORS_ORIGIN;
    process.env.CLIENT_PUBLIC_PORT = '5174';
  });

  afterEach(() => {
    process.chdir(cwd);
    process.env = { ...prev };
    fs.rmSync(tmp, { recursive: true, force: true });
  });

  it('falls back to local gateway', () => {
    expect(resolveClientPublicUrl()).toBe('http://127.0.0.1:5174');
  });

  it('prefers https CLIENT_CORS_ORIGIN', () => {
    process.env.CLIENT_CORS_ORIGIN = 'http://127.0.0.1:5174,https://clients.example.com';
    expect(resolveClientPublicUrl()).toBe('https://clients.example.com');
  });

  it('saves and reloads from marker', () => {
    const r = setClientPublicUrl('https://abc.trycloudflare.com/');
    expect(r.ok).toBe(true);
    if (r.ok) expect(r.url).toBe('https://abc.trycloudflare.com');
    delete process.env.CLIENT_PUBLIC_URL;
    loadClientPublicUrlFromDisk();
    expect(resolveClientPublicUrl()).toBe('https://abc.trycloudflare.com');
    expect(getClientWebPublicState().url).toBe('https://abc.trycloudflare.com');
    expect(process.env.CLIENT_CORS_ORIGIN || '').toContain('https://abc.trycloudflare.com');
  });
});
