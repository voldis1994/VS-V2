#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
python3 -m pip install -r "$ROOT/requirements/python/requirements-ml.txt"
python3 -m pip install -r "$ROOT/requirements/python/requirements-training.txt"
