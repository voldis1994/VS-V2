/**
 * Public Client Web homepage URL — what admins copy and send to clients.
 * Source order: marker file → CLIENT_PUBLIC_URL env → first https CLIENT_CORS_ORIGIN → local :5174
 */
import fs from 'node:fs';
import path from 'node:path';

const MARKER = '.vs-v2-client-public-url';

/** Candidate directories for the marker (repo root preferred; cwd may be apps/control-api). */
export function markerCandidateDirs(): string[] {
  const dirs: string[] = [];
  const add = (d: string | undefined | null) => {
    if (!d) return;
    const abs = path.resolve(d);
    if (!dirs.includes(abs)) dirs.push(abs);
  };
  add(process.env.VS_V2_ROOT);
  add(process.cwd());
  // Walk up from cwd looking for package.json + apps/control-api
  let cur = process.cwd();
  for (let i = 0; i < 6; i++) {
    add(cur);
    const parent = path.dirname(cur);
    if (parent === cur) break;
    cur = parent;
  }
  return dirs;
}

export function markerPaths(): string[] {
  return markerCandidateDirs().map((d) => path.join(d, MARKER));
}

function markerPath(): string {
  // Prefer existing marker; else write under VS_V2_ROOT or cwd
  for (const p of markerPaths()) {
    if (fs.existsSync(p)) return p;
  }
  const root = process.env.VS_V2_ROOT
    ? path.resolve(process.env.VS_V2_ROOT)
    : process.cwd();
  return path.join(root, MARKER);
}

function normalizeUrl(raw: string): string {
  let u = String(raw || '').trim();
  if (!u) return '';
  // Allow bare host → https
  if (!/^https?:\/\//i.test(u)) {
    u = `https://${u}`;
  }
  try {
    const parsed = new URL(u);
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

export function isPublicClientUrl(url: string): boolean {
  const u = normalizeUrl(url);
  if (!u) return false;
  try {
    const parsed = new URL(u);
    if (parsed.protocol !== 'https:') return false;
    const host = parsed.hostname.toLowerCase();
    if (host === 'localhost' || host === '127.0.0.1' || host === '::1') return false;
    return true;
  } catch {
    return false;
  }
}

/** Read marker into process.env (call on startup and on each GET so LIVE tunnel updates appear). */
export function loadClientPublicUrlFromDisk(): string | null {
  try {
    for (const p of markerPaths()) {
      if (!fs.existsSync(p)) continue;
      const raw = fs.readFileSync(p, 'utf8').trim();
      if (!raw) continue;
      const url = normalizeUrl(raw);
      if (!url) continue;
      process.env.CLIENT_PUBLIC_URL = url;
      return url;
    }
  } catch {
    /* ignore */
  }
  return null;
}

export function resolveClientPublicUrl(): string {
  // Always refresh from disk first so Cloudflare URL written by LIVE.bat appears without API restart.
  loadClientPublicUrlFromDisk();
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
  is_public: boolean;
  hint: string;
} {
  loadClientPublicUrlFromDisk();
  const marker = markerPath();
  let source: 'env' | 'cors' | 'local' | 'marker' = 'local';
  if (process.env.CLIENT_PUBLIC_URL) {
    source = fs.existsSync(marker) || markerPaths().some((p) => fs.existsSync(p))
      ? 'marker'
      : 'env';
  } else if (firstHttpsCorsOrigin()) {
    source = 'cors';
  }
  const url = resolveClientPublicUrl();
  const isPublic = isPublicClientUrl(url);
  return {
    url,
    source: isPublic || source !== 'local' ? source : 'local',
    local_gateway: localGatewayUrl(),
    editable: true,
    is_public: isPublic,
    hint: isPublic
      ? 'Public HTTPS URL — copy and send to clients.'
      : 'No Cloudflare / public HTTPS yet. LIVE.bat starts a quick tunnel automatically; or paste https://….trycloudflare.com below and SAVE.',
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
    const target = markerPath();
    fs.mkdirSync(path.dirname(target), { recursive: true });
    fs.writeFileSync(target, `${url}\n`, 'utf8');
    // Also write to VS_V2_ROOT / cwd peers so both API and LIVE.bat see it
    for (const p of markerPaths()) {
      if (p === target) continue;
      try {
        // Only mirror into dirs that look like the repo root
        const dir = path.dirname(p);
        if (
          fs.existsSync(path.join(dir, 'package.json')) &&
          fs.existsSync(path.join(dir, 'apps', 'control-api'))
        ) {
          fs.writeFileSync(p, `${url}\n`, 'utf8');
        }
      } catch {
        /* ignore mirror failures */
      }
    }
  } catch (err) {
    const message = err instanceof Error ? err.message : 'Failed to persist URL';
    return { ok: false, error: message };
  }

  return { ok: true, url };
}
