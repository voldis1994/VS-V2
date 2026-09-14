#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
npm update || true
python3 -m pip install -U -r requirements/python/requirements.txt
