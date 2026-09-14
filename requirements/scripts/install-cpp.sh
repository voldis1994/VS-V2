#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
echo "C++ deps via system packages or vcpkg ($ROOT/requirements/cpp/vcpkg.json)"
if command -v apt-get >/dev/null 2>&1; then
  sudo apt-get install -y libfmt-dev libspdlog-dev libyaml-cpp-dev nlohmann-json3-dev libgtest-dev || true
fi
