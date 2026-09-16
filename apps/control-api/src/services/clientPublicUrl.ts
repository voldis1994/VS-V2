/**
 * Public Client Web homepage URL — what admins copy and send to clients.
 * Source order: marker file → CLIENT_PUBLIC_URL env → first https CLIENT_CORS_ORIGIN → local :5174
 *
 * trycloudflare.com URLs die when the tunnel process exits. We probe reachability and
 * clear dead markers so iPhone Safari does not keep a hostname that DNS no longer resolves.
 */
import fs from 'node:fs';
import path from 'node:path';
import dns from 'node:dns/promises';

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

export function isTryCloudflareUrl(url: string): boolean {
  try {
    return new URL(normalizeUrl(url)).hostname.toLowerCase().endsWith('.trycloudflare.com');
  } catch {
    return /trycloudflare\.com/i.test(url);
  }
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

export type ClientWebPublicState = {
  url: string;
  source: 'env' | 'cors' | 'local' | 'marker';
  local_gateway: string;
  editable: true;
  is_public: boolean;
  reachable: boolean | null;
  stale: boolean;
  hint: string;
};

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

function stripCorsOrigin(url: string): void {
  let origin = url;
  try {
    origin = new URL(url).origin;
  } catch {
    /* keep */
  }
  const cur = (process.env.CLIENT_CORS_ORIGIN || '')
    .split(',')
    .map((s) => s.trim())
    .filter(Boolean)
    .filter((c) => c.toLowerCase() !== origin.toLowerCase());
  process.env.CLIENT_CORS_ORIGIN = cur.join(',');
}

/** Clear persisted public URL (used when trycloudflare tunnel dies). */
export function clearClientPublicUrl(opts?: { onlyTryCloudflare?: boolean }): {
  cleared: boolean;
  previous: string | null;
} {
  const prev = normalizeUrl(process.env.CLIENT_PUBLIC_URL || '') || loadClientPublicUrlFromDisk();
  if (!prev) {
    return { cleared: false, previous: null };
  }
  if (opts?.onlyTryCloudflare && !isTryCloudflareUrl(prev)) {
    return { cleared: false, previous: prev };
  }
  delete process.env.CLIENT_PUBLIC_URL;
  stripCorsOrigin(prev);
  for (const p of markerPaths()) {
    try {
      if (fs.existsSync(p)) fs.unlinkSync(p);
    } catch {
      /* ignore */
    }
  }
  // Also clear companion text file written by LIVE tunnel runner
  try {
    const root = process.env.VS_V2_ROOT ? path.resolve(process.env.VS_V2_ROOT) : process.cwd();
    const txt = path.join(root, 'logs', 'client-public-url.txt');
    if (fs.existsSync(txt)) fs.unlinkSync(txt);
  } catch {
    /* ignore */
  }
  return { cleared: true, previous: prev };
}

/**
 * Quick reachability check for public client URL.
 * trycloudflare hostnames stop resolving in DNS when the quick tunnel ends — that is
 * exactly the iPhone Safari "server can't be found" failure.
 */
export async function probeClientPublicUrl(
  url: string,
  timeoutMs = 3500
): Promise<{ reachable: boolean; detail: string }> {
  const u = normalizeUrl(url);
  if (!u || !isPublicClientUrl(u)) {
    return { reachable: false, detail: 'not a public https URL' };
  }
  let host = '';
  try {
    host = new URL(u).hostname;
  } catch {
    return { reachable: false, detail: 'invalid URL' };
  }

  try {
    await Promise.race([
      dns.lookup(host),
      new Promise((_, rej) => setTimeout(() => rej(new Error('dns timeout')), timeoutMs)),
    ]);
  } catch (err) {
    const msg = err instanceof Error ? err.message : String(err);
    return {
      reachable: false,
      detail: /ENOTFOUND|EAI_AGAIN|dns timeout|getaddrinfo/i.test(msg)
        ? 'DNS: hostname not found (tunnel dead — open VS-Cloudflare for a new URL)'
        : `DNS failed: ${msg}`,
    };
  }

  // Optional HTTP probe — some networks block HEAD; treat DNS OK as reachable for stable domains.
  if (!isTryCloudflareUrl(u)) {
    return { reachable: true, detail: 'dns ok' };
  }

  const ac = new AbortController();
  const t = setTimeout(() => ac.abort(), timeoutMs);
  try {
    const res = await fetch(u, {
      method: 'GET',
      redirect: 'follow',
      signal: ac.signal,
      headers: { Accept: 'text/html,*/*' },
    });
    // Any HTTP response means the tunnel edge answered (even 502 from dead local target).
    if (res.status > 0) {
      return { reachable: true, detail: `http ${res.status}` };
    }
    return { reachable: false, detail: 'empty http response' };
  } catch (err) {
    const msg = err instanceof Error ? err.message : String(err);
    // DNS already passed — tunnel hostname exists but fetch failed (local :5174 down, etc.)
    if (/abort|timeout/i.test(msg)) {
      return { reachable: false, detail: 'http timeout (tunnel or Client Web :5174 down)' };
    }
    return { reachable: false, detail: `http failed: ${msg}` };
  } finally {
    clearTimeout(t);
  }
}

function baseState(): ClientWebPublicState {
  loadClientPublicUrlFromDisk();
  const marker = markerPath();
  let source: ClientWebPublicState['source'] = 'local';
  if (process.env.CLIENT_PUBLIC_URL) {
    source =
      fs.existsSync(marker) || markerPaths().some((p) => fs.existsSync(p)) ? 'marker' : 'env';
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
    reachable: isPublic ? null : true,
    stale: false,
    hint: isPublic
      ? 'Public HTTPS URL — copy and send to clients. Keep VS-Cloudflare open.'
      : 'No Cloudflare / public HTTPS yet. LIVE.bat starts a quick tunnel automatically; or paste https://….trycloudflare.com below and SAVE.',
  };
}

export function getClientWebPublicState(): ClientWebPublicState {
  return baseState();
}

/** Async: probe trycloudflare and clear marker if DNS is dead. */
export async function getClientWebPublicStateProbed(): Promise<ClientWebPublicState> {
  const state = baseState();
  if (!state.is_public) return state;
  if (!isTryCloudflareUrl(state.url)) {
    return { ...state, reachable: true, stale: false };
  }

  const probe = await probeClientPublicUrl(state.url);
  if (probe.reachable) {
    return {
      ...state,
      reachable: true,
      stale: false,
      hint: 'Cloudflare tunnel reachable — copy URL for iPhone. Keep VS-Cloudflare window open.',
    };
  }

  // Dead trycloudflare — clear so Clients page stops advertising a ghost hostname.
  clearClientPublicUrl({ onlyTryCloudflare: true });
  const cleared = baseState();
  return {
    ...cleared,
    reachable: false,
    stale: true,
    is_public: false,
    hint: `Iepriekšējā Cloudflare adrese MIRUSI (${probe.detail}). Atver VS-Cloudflare logu, paņem JAUNO https://….trycloudflare.com, REFRESH URL → COPY URL. iPhone: izdzēs veco bookmark.`,
  };
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
