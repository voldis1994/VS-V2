#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
"$ROOT/requirements/scripts/install-system.sh"
"$ROOT/requirements/scripts/install-cpp.sh"
"$ROOT/requirements/scripts/install-python.sh"
"$ROOT/requirements/scripts/install-node.sh"
"$ROOT/requirements/scripts/install-database.sh"
echo "VS-V2 dependencies installed"
