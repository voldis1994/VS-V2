/**
 * Public Client Control Panel (port 5174).
 *
 * Serves the Vite *build* (dist-client) and proxies /api + /ws to Control API.
 * Vite is NOT in this path, so Cloudflare's changing *.trycloudflare.com
 * Host header can never trigger "Blocked request / allowedHosts".
 *
 * Safari/iPhone notes:
 * - Must be reached via https://*.trycloudflare.com (not 127.0.0.1)
 * - Proxy strips hop-by-hop headers (Safari rejects bad Transfer-Encoding combos)
 * - No Google Fonts CDN dependency in the client HTML build
 */
import http from 'node:http';
import fs from 'node:fs';
import net from 'node:net';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const DIR = path.dirname(fileURLToPath(import.meta.url));
const LISTEN_PORT = Number(process.env.CLIENT_PUBLIC_PORT || 5174);
const API_HOST = process.env.CONTROL_API_HOST || '127.0.0.1';
const API_PORT = Number(process.env.CONTROL_API_PORT || 3000);
const DIST = path.resolve(process.env.CLIENT_DIST || path.join(DIR, 'dist-client'));

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.ico': 'image/x-icon',
  '.woff': 'font/woff',
  '.woff2': 'font/woff2',
  '.map': 'application/json',
  '.txt': 'text/plain; charset=utf-8',
};

const HOP_BY_HOP = new Set([
  'connection',
  'keep-alive',
  'proxy-authenticate',
  'proxy-authorization',
  'proxy-connection',
  'te',
  'trailers',
  'transfer-encoding',
  'upgrade',
]);

function isApiPath(url) {
  const p = (url || '/').split('?')[0];
  return p === '/api' || p.startsWith('/api/') || p === '/ws' || p.startsWith('/ws/');
}

function forwardedProto(req) {
  const xf = String(req.headers['x-forwarded-proto'] || '')
    .split(',')[0]
    .trim()
    .toLowerCase();
  if (xf === 'http' || xf === 'https') return xf;
  const cf = String(req.headers['cf-visitor'] || '');
  if (/https/i.test(cf)) return 'https';
  if (/http/i.test(cf)) return 'http';
  // Cloudflare Tunnel terminates TLS; default https for public hosts.
  const host = String(req.headers.host || '');
  if (/trycloudflare\.com|cloudflare|.\../i.test(host) && !/^(localhost|127\.0\.0\.1)/i.test(host)) {
    return 'https';
  }
  return 'http';
}

function proxyHeaders(req) {
  const headers = {};
  for (const [key, value] of Object.entries(req.headers)) {
    const k = key.toLowerCase();
    if (HOP_BY_HOP.has(k)) continue;
    if (value === undefined) continue;
    headers[key] = value;
  }
  headers.host = `${API_HOST}:${API_PORT}`;
  headers['x-forwarded-host'] = req.headers.host || '';
  headers['x-forwarded-proto'] = forwardedProto(req);
  headers['x-forwarded-for'] = String(
    req.headers['cf-connecting-ip'] ||
      req.headers['x-real-ip'] ||
      req.socket.remoteAddress ||
      ''
  );
  return headers;
}

function filterResponseHeaders(incoming) {
  const out = {};
  for (const [key, value] of Object.entries(incoming.headers || {})) {
    const k = key.toLowerCase();
    if (HOP_BY_HOP.has(k)) continue;
    if (k === 'content-length' && incoming.headers['transfer-encoding']) continue;
    out[key] = value;
  }
  // Help Safari cache HTML/assets sanely through the tunnel.
  if (!out['cache-control'] && !out['Cache-Control']) {
    out['Cache-Control'] = 'no-store';
  }
  return out;
}

function proxyHttp(req, res) {
  const p = http.request(
    {
      hostname: API_HOST,
      port: API_PORT,
      path: req.url,
      method: req.method,
      headers: proxyHeaders(req),
    },
    (incoming) => {
      res.writeHead(incoming.statusCode || 502, filterResponseHeaders(incoming));
      incoming.pipe(res);
    },
  );
  p.on('error', (err) => {
    if (!res.headersSent) {
      res.writeHead(502, {
        'Content-Type': 'text/html; charset=utf-8',
        'Cache-Control': 'no-store',
      });
    }
    res.end(
      `<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1"/>` +
        `<body style="font-family:system-ui;padding:1.5rem;background:#111;color:#eee">` +
        `<h1>API offline</h1><p>Control API (:${API_PORT}) nav pieejams.</p>` +
        `<p>Uz Windows: palaid <b>LIVE.bat</b> / <b>Restart-ControlAPI.bat</b>.</p>` +
        `<pre style="opacity:.7">${String(err.message || err)}</pre></body>\n`,
    );
  });
  req.pipe(p);
}

function proxyUpgrade(req, clientSocket, head) {
  const proxy = net.connect(API_PORT, API_HOST, () => {
    const headers = proxyHeaders(req);
    let msg = `GET ${req.url || '/'} HTTP/1.1\r\n`;
    for (const [key, value] of Object.entries(headers)) {
      if (value === undefined) continue;
      if (Array.isArray(value)) {
        for (const item of value) msg += `${key}: ${item}\r\n`;
      } else {
        msg += `${key}: ${value}\r\n`;
      }
    }
    msg += '\r\n';
    proxy.write(msg);
    if (head && head.length) proxy.write(head);
    proxy.pipe(clientSocket);
    clientSocket.pipe(proxy);
  });
  proxy.on('error', () => clientSocket.destroy());
  clientSocket.on('error', () => proxy.destroy());
}

function safeFileFromUrl(urlPath) {
  const rel = decodeURIComponent((urlPath || '/').split('?')[0]);
  const fallback =
    fs.existsSync(path.join(DIST, 'index.html'))
      ? '/index.html'
      : '/index.client.html';
  const candidate = path.resolve(DIST, '.' + (rel === '/' ? fallback : rel));
  if (!candidate.startsWith(DIST)) return null;
  return candidate;
}

function sendFile(res, filePath) {
  const ext = path.extname(filePath).toLowerCase();
  const headers = {
    'Content-Type': MIME[ext] || 'application/octet-stream',
    'Cache-Control': ext === '.html' ? 'no-store' : 'public, max-age=300',
    // Avoid MIME sniffing quirks on mobile Safari.
    'X-Content-Type-Options': 'nosniff',
  };
  res.writeHead(200, headers);
  fs.createReadStream(filePath).pipe(res);
}

function sendIndexOrHelp(res) {
  const indexHtml = path.join(DIST, 'index.html');
  const indexClient = path.join(DIST, 'index.client.html');
  if (fs.existsSync(indexHtml)) {
    sendFile(res, indexHtml);
    return;
  }
  if (fs.existsSync(indexClient)) {
    sendFile(res, indexClient);
    return;
  }
  res.writeHead(503, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store' });
  res.end(
    `<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1"/>` +
      `<body style="font-family:system-ui;padding:1.5rem;background:#111;color:#eee">` +
      `<h1>Client panel not built</h1>` +
      `<p>Palaid <b>LIVE.bat</b> vai <b>ClientWeb.bat</b> (build:client).</p></body>\n`,
  );
}

const server = http.createServer((req, res) => {
  const pathOnly = (req.url || '/').split('?')[0];
  if (pathOnly === '/healthz' || pathOnly === '/health') {
    res.writeHead(200, {
      'Content-Type': 'text/plain; charset=utf-8',
      'Cache-Control': 'no-store',
    });
    res.end('ok\n');
    return;
  }
  if (isApiPath(req.url)) {
    proxyHttp(req, res);
    return;
  }
  const file = safeFileFromUrl(req.url);
  if (file && fs.existsSync(file) && fs.statSync(file).isFile()) {
    sendFile(res, file);
    return;
  }
  sendIndexOrHelp(res);
});

server.on('upgrade', (req, socket, head) => {
  if (isApiPath(req.url)) {
    proxyUpgrade(req, socket, head);
    return;
  }
  socket.destroy();
});

server.listen(LISTEN_PORT, '0.0.0.0', () => {
  const ready =
    fs.existsSync(path.join(DIST, 'index.html')) ||
    fs.existsSync(path.join(DIST, 'index.client.html'));
  console.log(
    `[client-gateway] public :${LISTEN_PORT}  static=${DIST}  api=${API_HOST}:${API_PORT}  built=${ready}`,
  );
});
