#!/bin/bash
set -euo pipefail
cmake --preset default
cmake --build --preset default
npm run build --workspaces
