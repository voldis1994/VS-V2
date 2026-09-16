import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {
  resolveClientPublicUrl,
  setClientPublicUrl,
  getClientWebPublicState,
  loadClientPublicUrlFromDisk,
  isPublicClientUrl,
  clearClientPublicUrl,
  isTryCloudflareUrl,
} from './clientPublicUrl.js';

describe('clientPublicUrl', () => {
  const prev = { ...process.env };
  let tmp: string;
  let cwd: string;

  beforeEach(() => {
    tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'vs-client-url-'));
    // Fake repo root shape so marker mirrors work
    fs.mkdirSync(path.join(tmp, 'apps', 'control-api'), { recursive: true });
    fs.writeFileSync(path.join(tmp, 'package.json'), '{"name":"vs-v2"}\n');
    cwd = process.cwd();
    process.chdir(tmp);
    process.env.VS_V2_ROOT = tmp;
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
    expect(getClientWebPublicState().is_public).toBe(false);
  });

  it('prefers https CLIENT_CORS_ORIGIN', () => {
    process.env.CLIENT_CORS_ORIGIN = 'http://127.0.0.1:5174,https://clients.example.com';
    expect(resolveClientPublicUrl()).toBe('https://clients.example.com');
    expect(isPublicClientUrl('https://clients.example.com')).toBe(true);
  });

  it('saves and reloads from marker without API restart', () => {
    const r = setClientPublicUrl('https://abc.trycloudflare.com/');
    expect(r.ok).toBe(true);
    if (r.ok) expect(r.url).toBe('https://abc.trycloudflare.com');
    delete process.env.CLIENT_PUBLIC_URL;
    loadClientPublicUrlFromDisk();
    expect(resolveClientPublicUrl()).toBe('https://abc.trycloudflare.com');
    const state = getClientWebPublicState();
    expect(state.url).toBe('https://abc.trycloudflare.com');
    expect(state.is_public).toBe(true);
    expect(process.env.CLIENT_CORS_ORIGIN || '').toContain('https://abc.trycloudflare.com');
  });

  it('picks up marker written externally (LIVE.bat tunnel)', () => {
    fs.writeFileSync(
      path.join(tmp, '.vs-v2-client-public-url'),
      'https://likes-option-tension.trycloudflare.com\n'
    );
    delete process.env.CLIENT_PUBLIC_URL;
    const state = getClientWebPublicState();
    expect(state.url).toBe('https://likes-option-tension.trycloudflare.com');
    expect(state.is_public).toBe(true);
    expect(state.source).toBe('marker');
  });

  it('rejects localhost as public', () => {
    expect(isPublicClientUrl('http://127.0.0.1:5174')).toBe(false);
    expect(isPublicClientUrl('https://localhost')).toBe(false);
  });

  it('detects trycloudflare and clears dead public URL', () => {
    expect(isTryCloudflareUrl('https://comfort-trips-reid-browser.trycloudflare.com')).toBe(true);
    setClientPublicUrl('https://comfort-trips-reid-browser.trycloudflare.com');
    expect(fs.existsSync(path.join(tmp, '.vs-v2-client-public-url'))).toBe(true);
    const cleared = clearClientPublicUrl({ onlyTryCloudflare: true });
    expect(cleared.cleared).toBe(true);
    expect(process.env.CLIENT_PUBLIC_URL || '').toBe('');
    expect(fs.existsSync(path.join(tmp, '.vs-v2-client-public-url'))).toBe(false);
    expect(resolveClientPublicUrl()).toBe('http://127.0.0.1:5174');
  });

  it('does not clear stable non-trycloudflare URL when onlyTryCloudflare', () => {
    setClientPublicUrl('https://clients.example.com');
    const cleared = clearClientPublicUrl({ onlyTryCloudflare: true });
    expect(cleared.cleared).toBe(false);
    expect(resolveClientPublicUrl()).toBe('https://clients.example.com');
  });
});
