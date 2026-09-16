/**
 * Public Client Web homepage URL — what admins copy and send to clients.
 * Source order: marker file → CLIENT_PUBLIC_URL env → first https CLIENT_CORS_ORIGIN → local :5174
 */
import fs from 'node:fs';
import path from 'node:path';

const MARKER = '.vs-v2-client-public-url';

function markerPath(): string {
  return path.resolve(process.cwd(), MARKER);
}

function normalizeUrl(raw: string): string {
  let u = String(raw || '').trim();
  if (!u) return '';
  // Allow bare host → https
  if (!/^https?:\/\//i.test(u)) {
    u = `https://${u}`;
  }
  // Strip trailing junk except single trailing slash preference: keep path, ensure no double slash end except root
  try {
    const parsed = new URL(u);
    // Prefer origin + pathname without trailing slash noise, but keep root as /
    let out = parsed.origin;
    if (parsed.pathname && parsed.pathname !== '/') {
      out += parsed.pathname.replace(/\/+$/, '');
    }
    return out;
  } catch {
    return u.replace(/\/+$/, '');
  }
}

function firstHttpsCorsOrigin(): string | null {
  const raw = process.env.CLIENT_CORS_ORIGIN || '';
  for (const part of raw.split(',')) {
    const t = part.trim();
    if (/^https:\/\//i.test(t)) return normalizeUrl(t);
  }
  return null;
}

function localGatewayUrl(): string {
  const port = process.env.CLIENT_PUBLIC_PORT || '5174';
  return `http://127.0.0.1:${port}`;
}

export function loadClientPublicUrlFromDisk(): void {
  try {
    const p = markerPath();
    if (!fs.existsSync(p)) return;
    const raw = fs.readFileSync(p, 'utf8').trim();
    if (!raw) return;
    const url = normalizeUrl(raw);
    if (url) process.env.CLIENT_PUBLIC_URL = url;
  } catch {
    /* ignore */
  }
}

export function resolveClientPublicUrl(): string {
  const fromEnv = normalizeUrl(process.env.CLIENT_PUBLIC_URL || '');
  if (fromEnv) return fromEnv;
  const fromCors = firstHttpsCorsOrigin();
  if (fromCors) return fromCors;
  return localGatewayUrl();
}

export function getClientWebPublicState(): {
  url: string;
  source: 'env' | 'cors' | 'local' | 'marker';
  local_gateway: string;
  editable: true;
} {
  const marker = markerPath();
  let source: 'env' | 'cors' | 'local' | 'marker' = 'local';
  if (process.env.CLIENT_PUBLIC_URL) {
    source = fs.existsSync(marker) ? 'marker' : 'env';
  } else if (firstHttpsCorsOrigin()) {
    source = 'cors';
  }
  return {
    url: resolveClientPublicUrl(),
    source,
    local_gateway: localGatewayUrl(),
    editable: true,
  };
}

/** Ensure CLIENT_CORS_ORIGIN includes the public URL (credentials / fetch). */
function ensureCorsIncludes(url: string): void {
  const origin = (() => {
    try {
      return new URL(url).origin;
    } catch {
      return url;
    }
  })();
  const cur = (process.env.CLIENT_CORS_ORIGIN || '')
    .split(',')
    .map((s) => s.trim())
    .filter(Boolean);
  if (!cur.some((c) => c.toLowerCase() === origin.toLowerCase())) {
    cur.push(origin);
    process.env.CLIENT_CORS_ORIGIN = cur.join(',');
  }
}

export function setClientPublicUrl(raw: string): { ok: true; url: string } | { ok: false; error: string } {
  const url = normalizeUrl(raw);
  if (!url) {
    return { ok: false, error: 'URL required' };
  }
  if (!/^https?:\/\//i.test(url)) {
    return { ok: false, error: 'URL must start with http:// or https://' };
  }
  try {
    // eslint-disable-next-line no-new
    new URL(url);
  } catch {
    return { ok: false, error: 'Invalid URL' };
  }

  process.env.CLIENT_PUBLIC_URL = url;
  ensureCorsIncludes(url);

  try {
    fs.writeFileSync(markerPath(), `${url}\n`, 'utf8');
  } catch (err) {
    const message = err instanceof Error ? err.message : 'Failed to persist URL';
    return { ok: false, error: message };
  }

  return { ok: true, url };
}
