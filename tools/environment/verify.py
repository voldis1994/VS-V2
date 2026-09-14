#!/usr/bin/env python3
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
required = [
  "CMakeLists.txt", "apps/market-core", "apps/control-api", "apps/dashboard",
  "libs/candle-engine", "libs/brain-core", "libs/decision-engine", "ai/learning",
]
missing = [r for r in required if not (ROOT/r).exists()]
if missing:
    print("MISSING:", ", ".join(missing))
    raise SystemExit(1)
print("VS-V2 layout OK")
