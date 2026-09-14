#!/bin/bash
set -euo pipefail
export OPERATING_MODE=LIVE LIVE_TRADING_ENABLED=false
./scripts/start-dev.sh
