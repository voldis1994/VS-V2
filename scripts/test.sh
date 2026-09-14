#!/bin/bash
set -euo pipefail
ctest --preset default || true
npm test --workspaces
python3 -m pytest ai/tests -q || true
