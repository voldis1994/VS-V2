#!/bin/bash
set -euo pipefail
export OPERATING_MODE=PAPER LIVE_TRADING_ENABLED=false
./scripts/start-dev.sh
