#!/usr/bin/env bash
# Safe PAPER deployment startup:
#   OPERATING_MODE=PAPER, LIVE_TRADING_ENABLED=false
#   Capital LIVE market data (if credentials set)
#   DB migrations → Control API → Market Core (--mode PAPER) → Dashboard
# Never switches SHADOW/LIVE and never places broker orders.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ -f .env.paper ]]; then
  set -a
  # shellcheck disable=SC1091
  source .env.paper
  set +a
elif [[ -f .env ]]; then
  set -a
  # shellcheck disable=SC1091
  source .env
  set +a
fi

export OPERATING_MODE=PAPER
export LIVE_TRADING_ENABLED=false
export MARKET_CORE_BRIDGE="${MARKET_CORE_BRIDGE:-true}"
export CAPITAL_BASE_URL="${CAPITAL_BASE_URL:-https://api-capital.backend-capital.com}"
export CONTROL_API_URL="${CONTROL_API_URL:-http://127.0.0.1:3000}"

mkdir -p logs

echo "==> PAPER deploy: forcing OPERATING_MODE=PAPER LIVE_TRADING_ENABLED=false"

# Infrastructure
if command -v docker >/dev/null 2>&1; then
  echo "==> Starting postgres + redis"
  docker compose -f infra/docker/docker-compose.yml up -d postgres redis
  # Wait for postgres
  for _ in $(seq 1 30); do
    if docker compose -f infra/docker/docker-compose.yml exec -T postgres \
      pg_isready -U "${DB_USER:-market_reader}" -d "${DB_NAME:-market_reader}" >/dev/null 2>&1; then
      break
    fi
    sleep 1
  done
fi

echo "==> DB migrations"
npm run migrate --workspace=@vs-v2/control-api

echo "==> Starting Control API"
npm run start --workspace=@vs-v2/control-api >logs/control-api.paper.log 2>&1 &
API_PID=$!
echo "$API_PID" >logs/control-api.paper.pid

# Wait for API health
for _ in $(seq 1 40); do
  if curl -fsS --max-time 2 "$CONTROL_API_URL/health" >/dev/null 2>&1; then
    break
  fi
  sleep 0.5
done

echo "==> PAPER preflight (fail-closed gates; Capital optional unless PAPER_REQUIRE_CAPITAL=1)"
bash scripts/preflight-paper.sh

echo "==> Starting Market Core (--mode PAPER, execution disabled)"
MC_BIN=""
if [[ -x build/apps/market-core/market-core ]]; then
  MC_BIN=build/apps/market-core/market-core
elif [[ -x apps/market-core/market-core ]]; then
  MC_BIN=apps/market-core/market-core
elif command -v market-core >/dev/null 2>&1; then
  MC_BIN=$(command -v market-core)
fi
if [[ -z "$MC_BIN" ]]; then
  echo "ERROR: market-core binary not found — build first (cmake --build build --target market-core)"
  kill "$API_PID" 2>/dev/null || true
  exit 1
fi
"$MC_BIN" --mode PAPER >logs/market-core.paper.log 2>&1 &
MC_PID=$!
echo "$MC_PID" >logs/market-core.paper.pid

echo "==> Starting Dashboard"
npm run dev --workspace=@vs-v2/dashboard >logs/dashboard.paper.log 2>&1 &
DASH_PID=$!
echo "$DASH_PID" >logs/dashboard.paper.pid

sleep 2
echo "==> Final preflight"
bash scripts/preflight-paper.sh || true

cat <<EOF

PAPER stack running (no SHADOW/LIVE switch, no broker orders):
  control-api  pid=$API_PID  log=logs/control-api.paper.log
  market-core  pid=$MC_PID   log=logs/market-core.paper.log  (--mode PAPER)
  dashboard    pid=$DASH_PID log=logs/dashboard.paper.log

Stop: kill \$(cat logs/*.paper.pid)
EOF
