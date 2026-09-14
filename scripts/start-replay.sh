#!/bin/bash
set -euo pipefail
./build/market-core --mode REPLAY --replay "$1"
