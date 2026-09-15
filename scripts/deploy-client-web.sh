#!/usr/bin/env bash
# Build + serve the public Client Control Panel (HTTPS-ready via tunnel / reverse proxy).
#
# Clients must reach this over the public internet — NOT LAN / Wi‑Fi only.
# Flow: dist-client static → client-gateway (:5174) → Control API (:3000)
# Optional: Cloudflare tunnel / nginx TLS in front of :5174.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

PUBLIC_PORT="${CLIENT_PUBLIC_PORT:-5174}"
API_PORT="${CONTROL_API_PORT:-3000}"
API_HOST="${CONTROL_API_HOST:-127.0.0.1}"

echo "==> Building client web (apps/dashboard → dist-client)"
npm run build:client --workspace=@vs-v2/dashboard

DIST="$ROOT/apps/dashboard/dist-client"
if [[ ! -f "$DIST/index.client.html" && ! -f "$DIST/index.html" ]]; then
  echo "ERROR: client build missing under $DIST"
  exit 1
fi

# Prefer a stable index.html entry for gateway / control-api static serve.
if [[ -f "$DIST/index.client.html" && ! -f "$DIST/index.html" ]]; then
  cp -f "$DIST/index.client.html" "$DIST/index.html"
  echo "==> Linked index.html ← index.client.html"
fi

export CLIENT_DIST="${CLIENT_DIST:-$DIST}"
export CLIENT_PANEL_DIST="${CLIENT_PANEL_DIST:-$DIST}"
export CLIENT_PUBLIC_PORT="$PUBLIC_PORT"
export CONTROL_API_HOST="$API_HOST"
export CONTROL_API_PORT="$API_PORT"
# Cookies over public HTTPS
export CLIENT_COOKIE_SECURE="${CLIENT_COOKIE_SECURE:-true}"
export TRUST_PROXY="${TRUST_PROXY:-true}"
# Comma-separated public origins (tunnel URL, custom domain). Empty = localhost default in API.
# export CLIENT_CORS_ORIGIN="https://your-tunnel.trycloudflare.com,https://clients.example.com"

mkdir -p "$ROOT/logs"

echo "==> Client panel ready at $CLIENT_DIST"
echo "    Local gateway:  http://127.0.0.1:${PUBLIC_PORT}/"
echo "    Proxies API to: http://${API_HOST}:${API_PORT}/"
echo "    Put Cloudflare / nginx TLS in front for public HTTPS."
echo ""
echo "Env checklist for public clients:"
echo "  CLIENT_COOKIE_SECURE=true"
echo "  TRUST_PROXY=true"
echo "  CLIENT_CORS_ORIGIN=<https public origin(s)>"
echo "  CLIENT_PANEL_DIST / CLIENT_DIST → dist-client"
echo ""

if [[ "${DEPLOY_CLIENT_START:-1}" == "0" ]]; then
  echo "==> Build only (DEPLOY_CLIENT_START=0) — skip gateway"
  exit 0
fi

# Health-check Control API (warn only — gateway still starts)
if curl -fsS --max-time 2 "http://${API_HOST}:${API_PORT}/health" >/dev/null 2>&1; then
  echo "==> Control API healthy on :${API_PORT}"
else
  echo "WARN: Control API not reachable on :${API_PORT} — start it before clients can log in"
fi

echo "==> Starting client-gateway on :${PUBLIC_PORT}"
exec npm run dev:client --workspace=@vs-v2/dashboard
