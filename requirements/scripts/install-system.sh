#!/usr/bin/env bash
set -euo pipefail
if command -v apt-get >/dev/null 2>&1; then
  sudo apt-get update
  sudo apt-get install -y build-essential cmake ninja-build git pkg-config curl ca-certificates libssl-dev zlib1g-dev libcurl4-openssl-dev
fi
