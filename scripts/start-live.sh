#!/usr/bin/env bash
# LIVE deployment startup:
#   OPERATING_MODE=LIVE, LIVE_TRADING_ENABLED=true
#   Capital execution bound via market-core --mode LIVE
# Requires explicit confirm: CONFIRM_LIVE=LIVE or interactive prompt.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

confirm="${CONFIRM_LIVE:-}"
if [[ "$confirm" != "LIVE" ]]; then
  echo "WARNING: LIVE arms real Capital.com broker open/close orders."
  read -r -p "Type LIVE to confirm: " confirm
fi
if [[ "$confirm" != "LIVE" ]]; then
  echo "Refusing LIVE start (confirmation failed)."
  echo "Safe PAPER path: ./scripts/start-paper.sh"
  exit 2
fi

if [[ -f .env.live ]]; then
  set -a
  # shellcheck disable=SC1091
  source .env.live
  set +a
elif [[ -f .env.paper ]]; then
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

export OPERATING_MODE=LIVE
export LIVE_TRADING_ENABLED=true
export MARKET_CORE_BRIDGE="${MARKET_CORE_BRIDGE:-true}"
export CAPITAL_BASE_URL="${CAPITAL_BASE_URL:-https://api-capital.backend-capital.com}"
export CONTROL_API_URL="${CONTROL_API_URL:-http://127.0.0.1:3000}"

for k in CAPITAL_API_KEY CAPITAL_API_PASSWORD CAPITAL_IDENTIFIER; do
  if [[ -z "${!k:-}" ]]; then
    echo "ERROR: LIVE requires $k in .env.live / .env"
    exit 1
  fi
done

mkdir -p logs
echo LIVE > .vs-v2-runtime-mode

echo "==> LIVE deploy: OPERATING_MODE=LIVE LIVE_TRADING_ENABLED=true"

if command -v docker >/dev/null 2>&1; then
  echo "==> Starting postgres + redis"
  docker compose -f infra/docker/docker-compose.yml up -d postgres redis
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

echo "==> Starting Control API (LIVE)"
npm run start --workspace=@vs-v2/control-api >logs/control-api.live.log 2>&1 &
API_PID=$!
echo "$API_PID" >logs/control-api.live.pid

for _ in $(seq 1 40); do
  if curl -fsS --max-time 2 "$CONTROL_API_URL/health" >/dev/null 2>&1; then
    break
  fi
  sleep 0.5
done

if ! curl -fsS --max-time 2 "$CONTROL_API_URL/health" >/dev/null 2>&1; then
  echo "ERROR: control-api /health failed"
  kill "$API_PID" 2>/dev/null || true
  exit 1
fi

echo "==> Starting Market Core (--mode LIVE, Capital execution bound)"
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
"$MC_BIN" --mode LIVE >logs/market-core.live.log 2>&1 &
MC_PID=$!
echo "$MC_PID" >logs/market-core.live.pid

echo "==> Starting Dashboard"
npm run dev --workspace=@vs-v2/dashboard >logs/dashboard.live.log 2>&1 &
DASH_PID=$!
echo "$DASH_PID" >logs/dashboard.live.pid

sleep 2

cat <<EOF

LIVE stack running (Capital open/close armed when gates pass):
  control-api  pid=$API_PID  log=logs/control-api.live.log
  market-core  pid=$MC_PID   log=logs/market-core.live.log  (--mode LIVE)
  dashboard    pid=$DASH_PID log=logs/dashboard.live.log

Stop: kill \$(cat logs/*.live.pid)
EOF
