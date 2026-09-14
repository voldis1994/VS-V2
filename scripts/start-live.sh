#!/bin/bash
set -euo pipefail
export OPERATING_MODE=LIVE LIVE_TRADING_ENABLED=true MARKET_CORE_BRIDGE=1
./scripts/start-dev.sh
