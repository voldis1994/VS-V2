#!/usr/bin/env bash
# PAPER preflight — fail-closed health gate before/after startup.
# Does NOT switch SHADOW/LIVE and does NOT place broker orders.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CONTROL_API_URL="${CONTROL_API_URL:-http://127.0.0.1:3000}"
REQUIRE_CAPITAL="${PAPER_REQUIRE_CAPITAL:-0}"

red() { printf '\033[31m%s\033[0m\n' "$*"; }
grn() { printf '\033[32m%s\033[0m\n' "$*"; }
ylw() { printf '\033[33m%s\033[0m\n' "$*"; }

fail=0
check() {
  local name="$1" ok="$2" detail="${3:-}"
  if [[ "$ok" == "1" ]]; then
    grn "OK  $name${detail:+ — $detail}"
  else
    red "FAIL $name${detail:+ — $detail}"
    fail=1
  fi
}

# --- Local env fail-closed ---
MODE="${OPERATING_MODE:-PAPER}"
LIVE="${LIVE_TRADING_ENABLED:-false}"
check "OPERATING_MODE=PAPER" "$([[ "${MODE^^}" == "PAPER" ]] && echo 1 || echo 0)" "got=$MODE"
check "LIVE_TRADING_ENABLED=false" "$([[ "${LIVE,,}" == "false" || "$LIVE" == "0" || -z "$LIVE" ]] && echo 1 || echo 0)" "got=$LIVE"

# --- Control API ---
if ! curl -fsS --max-time 5 "$CONTROL_API_URL/health" >/tmp/vs-paper-health.json 2>/dev/null; then
  check "control-api /health" 0 "unreachable at $CONTROL_API_URL"
else
  check "control-api /health" 1
fi

if curl -fsS --max-time 5 "$CONTROL_API_URL/api/system/preflight" >/tmp/vs-paper-preflight.json 2>/dev/null; then
  set +e
  python3 - <<'PY'
import json, sys
p = json.load(open("/tmp/vs-paper-preflight.json"))
ok = bool(p.get("ok"))
data_ready = bool(p.get("data_ready"))
print("preflight_ok", ok)
print("data_ready", data_ready)
print("mode", p.get("operating_mode"))
print("live_trading_enabled", p.get("live_trading_enabled"))
print("live_entries_allowed", p.get("live_entries_allowed"))
print("broker_orders_forbidden", p.get("broker_orders_forbidden"))
print("checks", json.dumps(p.get("checks", {}), sort_keys=True))
print("reasons", p.get("reasons"))
sys.exit(0 if ok else 1)
PY
  pf_rc=$?
  set -e
  if [[ "$pf_rc" -eq 0 ]]; then
    check "control-api /api/system/preflight fail-closed" 1
  else
    check "control-api /api/system/preflight fail-closed" 0 "$(cat /tmp/vs-paper-preflight.json)"
  fi
  if [[ "$REQUIRE_CAPITAL" == "1" ]]; then
    set +e
    python3 - <<'PY'
import json, sys
p = json.load(open("/tmp/vs-paper-preflight.json"))
sys.exit(0 if p.get("data_ready") else 1)
PY
    cap_rc=$?
    set -e
    check "Capital LIVE market-data ready" "$([[ "$cap_rc" -eq 0 ]] && echo 1 || echo 0)"
  fi
else
  # Fallback to status if preflight not yet deployed on running binary
  if curl -fsS --max-time 5 "$CONTROL_API_URL/api/system/status" >/tmp/vs-paper-status.json 2>/dev/null; then
    set +e
    python3 - <<'PY'
import json, sys
s = json.load(open("/tmp/vs-paper-status.json"))
mode = str(s.get("mode") or "").upper()
live = bool(s.get("live_enabled"))
entries = bool(s.get("live_entries_allowed"))
db = str(s.get("database") or "")
ok = mode == "PAPER" and not live and not entries and db == "HEALTHY"
print("status_mode", mode, "live_enabled", live, "entries", entries, "db", db)
sys.exit(0 if ok else 1)
PY
    st_rc=$?
    set -e
    check "control-api /api/system/status PAPER fail-closed" "$([[ "$st_rc" -eq 0 ]] && echo 1 || echo 0)"
  else
    check "control-api status/preflight" 0 "unreachable"
  fi
fi

# --- Capital credentials present for LIVE market data (optional until wired) ---
if [[ -n "${CAPITAL_API_KEY:-}" && -n "${CAPITAL_API_PASSWORD:-}" && -n "${CAPITAL_IDENTIFIER:-}" && -n "${CAPITAL_EPIC:-}" ]]; then
  check "Capital LIVE market-data credentials present" 1 "epic=$CAPITAL_EPIC"
  BASE="${CAPITAL_BASE_URL:-https://api-capital.backend-capital.com}"
  if [[ "$BASE" == *"demo"* ]]; then
    ylw "WARN Capital BASE_URL looks like demo ($BASE) — PAPER deploy expects LIVE market data host"
    if [[ "$REQUIRE_CAPITAL" == "1" ]]; then
      check "Capital BASE_URL is LIVE market-data host" 0 "$BASE"
    fi
  else
    check "Capital BASE_URL is LIVE market-data host" 1 "$BASE"
  fi
else
  ylw "WARN Capital credentials incomplete — market-core will idle without LIVE market data (still no orders)"
  if [[ "$REQUIRE_CAPITAL" == "1" ]]; then
    check "Capital LIVE market-data credentials present" 0
  fi
fi

if [[ "$fail" -ne 0 ]]; then
  red "PAPER preflight FAILED — refuse to continue"
  exit 1
fi
grn "PAPER preflight PASSED (broker orders remain forbidden)"
exit 0
