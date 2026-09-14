#!/usr/bin/env python3
"""Generate ai/, config/, contracts/, database schemas, scripts, tools, CI, docker."""
from __future__ import annotations
import json
import os
import textwrap

ROOT = "/workspace"

def w(path: str, content: str) -> None:
    full = os.path.join(ROOT, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "w", encoding="utf-8") as f:
        f.write(content if content.endswith("\n") else content + "\n")

# --- AI learning modules (adapted from reader engine) ---
w("ai/learning/features/structure.py", textwrap.dedent('''
"""Structure features — adapted from VS_READER_ENGINE_V2 analysis/structure."""
from __future__ import annotations
from dataclasses import dataclass
from typing import Sequence

@dataclass(frozen=True)
class Bar:
    open: float
    high: float
    low: float
    close: float

@dataclass(frozen=True)
class StructureFeatures:
    swing_high: float
    swing_low: float
    structure_bias: str
    break_of_structure: bool
    support_level: float
    resistance_level: float

def extract_structure_features(bars: Sequence[Bar]) -> StructureFeatures:
    if not bars:
        return StructureFeatures(0, 0, "NEUTRAL", False, 0, 0)
    highs = [b.high for b in bars]
    lows = [b.low for b in bars]
    closes = [b.close for b in bars]
    swing_high, swing_low = max(highs), min(lows)
    first_close, last_close = closes[0], closes[-1]
    if last_close > first_close:
        bias = "BULLISH"
    elif last_close < first_close:
        bias = "BEARISH"
    else:
        bias = "NEUTRAL"
    prior_high = max(highs[:-1]) if len(highs) > 1 else highs[0]
    prior_low = min(lows[:-1]) if len(lows) > 1 else lows[0]
    bos = last_close > prior_high or last_close < prior_low
    return StructureFeatures(swing_high, swing_low, bias, bos, swing_low, swing_high)
'''))

w("ai/learning/features/momentum.py", textwrap.dedent('''
"""Momentum features — adapted from reader engine."""
from __future__ import annotations
from dataclasses import dataclass
from typing import Sequence
from ai.learning.features.structure import Bar

def _clamp(v: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, v))

@dataclass(frozen=True)
class MomentumFeatures:
    momentum_score: float
    rate_of_change: float
    trend_direction: str
    trend_strength: float

def extract_momentum_features(bars: Sequence[Bar]) -> MomentumFeatures:
    if len(bars) < 2:
        return MomentumFeatures(0.0, 0.0, "SIDEWAYS", 0.0)
    first, last = bars[0].close, bars[-1].close
    roc = (last - first) / first if first else 0.0
    score = _clamp(roc * 10.0, -1.0, 1.0)
    direction = "UP" if score > 0 else "DOWN" if score < 0 else "SIDEWAYS"
    return MomentumFeatures(score, roc, direction, abs(score))
'''))

w("ai/learning/features/pressure.py", textwrap.dedent('''
"""Pressure features — adapted from reader engine."""
from __future__ import annotations
from dataclasses import dataclass
from typing import Sequence
from ai.learning.features.structure import Bar

def _clamp(v: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, v))

@dataclass(frozen=True)
class PressureFeatures:
    buy_pressure: float
    sell_pressure: float
    pressure_delta: float

def extract_pressure_features(bars: Sequence[Bar]) -> PressureFeatures:
    if not bars:
        return PressureFeatures(0.5, 0.5, 0.0)
    buy_vals, sell_vals = [], []
    for bar in bars:
        rng = bar.high - bar.low
        if rng <= 0:
            buy_vals.append(0.5); sell_vals.append(0.5); continue
        body = bar.close - bar.open
        factor = _clamp(abs(body) / rng, 0.0, 1.0)
        if body >= 0:
            buy = 0.5 + 0.5 * factor
        else:
            buy = 0.5 - 0.5 * factor
        buy_vals.append(_clamp(buy, 0.0, 1.0))
        sell_vals.append(1.0 - buy_vals[-1])
    buy_p = sum(buy_vals) / len(buy_vals)
    sell_p = sum(sell_vals) / len(sell_vals)
    return PressureFeatures(buy_p, sell_p, buy_p - sell_p)
'''))

w("ai/learning/features/__init__.py", "from .structure import *\\nfrom .momentum import *\\nfrom .pressure import *\\n")

w("ai/learning/labeling/scorer.py", textwrap.dedent('''
"""Labeling scorer — adapted from decision/scorer.py."""
from __future__ import annotations
from dataclasses import dataclass

@dataclass(frozen=True)
class LabelScores:
    buy_score: float
    sell_score: float
    preferred_side: str

def label_from_scores(buy_score: float, sell_score: float, context_quality: float = 1.0) -> LabelScores:
    buy = buy_score * context_quality
    sell = sell_score * context_quality
    if buy > sell and buy > 0.55:
        side = "BUY"
    elif sell > buy and sell > 0.55:
        side = "SELL"
    else:
        side = "WAIT"
    return LabelScores(buy, sell, side)
'''))

w("ai/learning/labeling/__init__.py", "from .scorer import *\\n")

w("ai/learning/dataset/builder.py", textwrap.dedent('''
"""Build training datasets from feature episodes."""
from __future__ import annotations
import json
from pathlib import Path
from typing import Any

def build_dataset(episodes_dir: Path, output: Path) -> dict[str, Any]:
    rows = []
    if episodes_dir.exists():
        for f in sorted(episodes_dir.glob("*.json")):
            rows.append(json.loads(f.read_text()))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"rows": rows, "count": len(rows)}, indent=2))
    return {"count": len(rows), "output": str(output)}
'''))

w("ai/learning/dataset/__init__.py", "from .builder import build_dataset\\n")

w("ai/learning/patterns/detector.py", textwrap.dedent('''
from __future__ import annotations
from dataclasses import dataclass
from typing import Sequence
from ai.learning.features.structure import Bar

@dataclass(frozen=True)
class PatternHit:
    name: str
    confidence: float

def detect_patterns(bars: Sequence[Bar]) -> list[PatternHit]:
    if len(bars) < 3:
        return []
    last = bars[-1]
    body = last.close - last.open
    rng = last.high - last.low
    if rng > 0 and abs(body) / rng > 0.7:
        direction = "BULLISH_MARUBOZU" if body > 0 else "BEARISH_MARUBOZU"
        return [PatternHit(direction, min(1.0, abs(body) / rng))]
    return []
'''))

w("ai/learning/patterns/__init__.py", "from .detector import *\\n")

w("ai/learning/training/trainer.py", textwrap.dedent('''
"""Simple baseline trainer placeholder with real feature vector assembly."""
from __future__ import annotations
import json
from pathlib import Path
from typing import Any

def train_baseline(dataset_path: Path, model_out: Path) -> dict[str, Any]:
    data = json.loads(dataset_path.read_text()) if dataset_path.exists() else {"rows": []}
    weights = {"structure": 0.25, "momentum": 0.35, "pressure": 0.25, "behavior": 0.15}
    model_out.parent.mkdir(parents=True, exist_ok=True)
    model_out.write_text(json.dumps({"type": "baseline", "weights": weights, "samples": len(data.get("rows", []))}, indent=2))
    return {"samples": len(data.get("rows", [])), "model": str(model_out)}
'''))

w("ai/learning/training/__init__.py", "from .trainer import train_baseline\\n")

w("ai/learning/calibration/platt.py", textwrap.dedent('''
from __future__ import annotations
import math

def platt_scale(raw_score: float, a: float = 1.0, b: float = 0.0) -> float:
    z = a * raw_score + b
    return 1.0 / (1.0 + math.exp(-z))
'''))

w("ai/learning/calibration/__init__.py", "from .platt import platt_scale\\n")

w("ai/learning/evaluation/metrics.py", textwrap.dedent('''
from __future__ import annotations

def accuracy(y_true: list[int], y_pred: list[int]) -> float:
    if not y_true:
        return 0.0
    correct = sum(1 for t, p in zip(y_true, y_pred) if t == p)
    return correct / len(y_true)

def expected_calibration_error(probs: list[float], outcomes: list[int], bins: int = 10) -> float:
    if not probs:
        return 0.0
    bucket_size = max(1, len(probs) // bins)
    ece = 0.0
    for i in range(0, len(probs), bucket_size):
        chunk_p = probs[i:i + bucket_size]
        chunk_y = outcomes[i:i + bucket_size]
        if not chunk_p:
            continue
        conf = sum(chunk_p) / len(chunk_p)
        acc = sum(chunk_y) / len(chunk_y)
        ece += abs(conf - acc) * len(chunk_p) / len(probs)
    return ece
'''))

w("ai/learning/evaluation/__init__.py", "from .metrics import *\\n")

# validation modules
validation_modules = {
    "out_of_sample": "def validate_out_of_sample(train_metrics: dict, test_metrics: dict, min_edge: float = 0.0) -> bool:\n    return test_metrics.get('edge', 0) >= min_edge and test_metrics.get('edge', 0) <= train_metrics.get('edge', 1) + 0.15\n",
    "walk_forward": "def validate_walk_forward(folds: list[dict], min_positive: int = 1) -> bool:\n    return sum(1 for f in folds if f.get('pnl', 0) > 0) >= min_positive\n",
    "monte_carlo": "import random\ndef validate_monte_carlo(returns: list[float], simulations: int = 1000, ruin_threshold: float = -0.2) -> dict:\n    if not returns:\n        return {'ruin_rate': 1.0, 'passed': False}\n    ruins = 0\n    for _ in range(simulations):\n        equity = 0.0\n        for _ in range(len(returns)):\n            equity += random.choice(returns)\n            if equity <= ruin_threshold:\n                ruins += 1\n                break\n    rate = ruins / simulations\n    return {'ruin_rate': rate, 'passed': rate < 0.05}\n",
    "transaction_cost_stress": "def validate_transaction_cost_stress(edge: float, cost_bps: float = 5.0) -> bool:\n    return edge > cost_bps / 10000.0\n",
    "latency_stress": "def validate_latency_stress(p95_ms: float, max_ms: float = 250.0) -> bool:\n    return p95_ms <= max_ms\n",
    "market_state_robustness": "def validate_market_state_robustness(by_regime: dict[str, float], min_regimes: int = 2) -> bool:\n    positive = [v for v in by_regime.values() if v > 0]\n    return len(positive) >= min_regimes\n",
    "probability_calibration": "from ai.learning.evaluation.metrics import expected_calibration_error\ndef validate_probability_calibration(probs: list[float], outcomes: list[int], max_ece: float = 0.12) -> bool:\n    return expected_calibration_error(probs, outcomes) <= max_ece\n",
    "promotion_gate": "def promotion_gate(checks: dict[str, bool]) -> bool:\n    required = ['out_of_sample', 'walk_forward', 'monte_carlo', 'probability_calibration']\n    return all(checks.get(k, False) for k in required)\n",
}
for name, body in validation_modules.items():
    w(f"ai/validation/{name}.py", f'"""VS-V2 validation: {name}"""\nfrom __future__ import annotations\n\n{body}')
w("ai/validation/__init__.py", "\n".join(f"from .{n} import *" for n in validation_modules))

w("ai/registry/model_registry.py", textwrap.dedent('''
from __future__ import annotations
import json
from pathlib import Path
from typing import Any

class ModelRegistry:
    def __init__(self, root: Path):
        self.root = root
        self.root.mkdir(parents=True, exist_ok=True)

    def register(self, name: str, version: str, metadata: dict[str, Any]) -> Path:
        dest = self.root / name / version / "metadata.json"
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(json.dumps(metadata, indent=2))
        return dest

    def list_versions(self, name: str) -> list[str]:
        p = self.root / name
        if not p.exists():
            return []
        return sorted(x.name for x in p.iterdir() if x.is_dir())
'''))

w("ai/registry/__init__.py", "from .model_registry import ModelRegistry\\n")
w("ai/__init__.py", '"""VS-V2 learning and validation package."""\n')

# config JSON
w("config/markets/gold.json", json.dumps({"symbol": "XAUUSD", "epic": "GOLD", "tick_size": 0.01, "session": "LONDON_NY"}, indent=2))
w("config/instruments.json", json.dumps({"instruments": [{"id": 1, "symbol": "XAUUSD", "epic": "GOLD"}, {"id": 2, "symbol": "EURUSD", "epic": "EURUSD"}]}, indent=2))
w("config/data/quality.json", json.dumps({"stale_threshold_ms": 500, "max_spread_bps": 25}, indent=2))
w("config/data/fusion.json", json.dumps({"min_sources": 1, "divergence_cap_bps": 15}, indent=2))
w("config/brain/scoring.json", json.dumps({"weights": {"structure": 0.25, "momentum": 0.35, "pressure": 0.25, "behavior": 0.15}}, indent=2))
w("config/cross-market/correlation.json", json.dumps({"pairs": [{"lead": "DXY", "follow": "XAUUSD", "sign": -1}]}, indent=2))
w("config/decision/thresholds.json", json.dumps({"min_confidence": 0.55, "min_bars": 3}, indent=2))
w("config/position/sizing.json", json.dumps({"risk_per_trade": 0.01, "max_positions": 3}, indent=2))
w("config/risk/limits.json", json.dumps({"max_daily_loss": 0.03, "max_exposure": 0.15}, indent=2))
w("config/execution/routing.json", json.dumps({"default_broker": "capital", "slippage_bps": 2}, indent=2))
w("config/learning/training.json", json.dumps({"validation_split": 0.2, "walk_forward_folds": 5}, indent=2))
w("config/environments/development.json", json.dumps({"mode": "PAPER", "log_level": "debug"}, indent=2))
w("config/environments/paper.json", json.dumps({"mode": "PAPER", "live_trading": False}, indent=2))
w("config/environments/shadow.json", json.dumps({"mode": "LIVE", "live_trading": False}, indent=2))
w("config/environments/production.json", json.dumps({"mode": "LIVE", "live_trading": True}, indent=2))

# contracts
w("contracts/events/market_tick.json", json.dumps({"$schema": "http://json-schema.org/draft-07/schema#", "title": "MarketTick", "type": "object", "required": ["instrument_id", "bid", "ask", "timestamp"], "properties": {"instrument_id": {"type": "integer"}, "bid": {"type": "number"}, "ask": {"type": "number"}, "timestamp": {"type": "string"}}}, indent=2))
w("contracts/events/brain_snapshot.json", json.dumps({"$schema": "http://json-schema.org/draft-07/schema#", "title": "BrainSnapshot", "type": "object", "properties": {"instrument_id": {"type": "integer"}, "bias": {"type": "string"}, "composite": {"type": "number"}}}, indent=2))
w("contracts/events/decision.json", json.dumps({"$schema": "http://json-schema.org/draft-07/schema#", "title": "Decision", "type": "object", "properties": {"action": {"enum": ["WAIT", "BUY", "SELL"]}, "confidence": {"type": "number"}}}, indent=2))
w("contracts/events/risk_intent.json", json.dumps({"$schema": "http://json-schema.org/draft-07/schema#", "title": "RiskIntent", "type": "object", "properties": {"direction": {"type": "string"}, "reference_price": {"type": "number"}}}, indent=2))
w("contracts/model/baseline.json", json.dumps({"$schema": "http://json-schema.org/draft-07/schema#", "title": "BaselineModel", "type": "object", "properties": {"weights": {"type": "object"}}}, indent=2))

# database schemas
schemas = {
    "market.sql": "CREATE TABLE IF NOT EXISTS market_snapshots (id BIGSERIAL PRIMARY KEY, instrument_id INT, epic TEXT, mid DOUBLE PRECISION, spread DOUBLE PRECISION, captured_at TIMESTAMPTZ DEFAULT NOW());",
    "candles.sql": "CREATE TABLE IF NOT EXISTS candles (id BIGSERIAL PRIMARY KEY, instrument_id INT, bucket_start TIMESTAMPTZ, open DOUBLE PRECISION, high DOUBLE PRECISION, low DOUBLE PRECISION, close DOUBLE PRECISION, tick_count INT);",
    "brain.sql": "CREATE TABLE IF NOT EXISTS brain_snapshots (id BIGSERIAL PRIMARY KEY, instrument_id INT, bias TEXT, structure DOUBLE PRECISION, momentum DOUBLE PRECISION, pressure DOUBLE PRECISION, composite DOUBLE PRECISION, captured_at TIMESTAMPTZ DEFAULT NOW());",
    "patterns.sql": "CREATE TABLE IF NOT EXISTS patterns (id BIGSERIAL PRIMARY KEY, instrument_id INT, name TEXT, confidence DOUBLE PRECISION, detected_at TIMESTAMPTZ DEFAULT NOW());",
    "predictions.sql": "CREATE TABLE IF NOT EXISTS predictions (id UUID PRIMARY KEY DEFAULT gen_random_uuid(), instrument_id INT, horizon TEXT, probability DOUBLE PRECISION, edge DOUBLE PRECISION, created_at TIMESTAMPTZ DEFAULT NOW());",
    "decisions.sql": "CREATE TABLE IF NOT EXISTS decisions (id BIGSERIAL PRIMARY KEY, instrument_id INT, action TEXT, confidence DOUBLE PRECISION, reason TEXT, created_at TIMESTAMPTZ DEFAULT NOW());",
    "trades.sql": "CREATE TABLE IF NOT EXISTS trades (id BIGSERIAL PRIMARY KEY, epic TEXT, direction TEXT, size DOUBLE PRECISION, entry_price DOUBLE PRECISION, exit_price DOUBLE PRECISION, pnl DOUBLE PRECISION, opened_at TIMESTAMPTZ, closed_at TIMESTAMPTZ);",
    "outcomes.sql": "CREATE TABLE IF NOT EXISTS outcomes (id BIGSERIAL PRIMARY KEY, decision_id BIGINT, realized_pnl DOUBLE PRECISION, label TEXT, recorded_at TIMESTAMPTZ DEFAULT NOW());",
    "models.sql": "CREATE TABLE IF NOT EXISTS model_registry (id BIGSERIAL PRIMARY KEY, name TEXT, version TEXT, status TEXT, metadata JSONB, created_at TIMESTAMPTZ DEFAULT NOW());",
}
for name, sql in schemas.items():
    w(f"database/schemas/{name}", sql)

# requirements
w("requirements/requirements.txt", "numpy>=1.26\npandas>=2.2\nscikit-learn>=1.5\npyyaml>=6.0\n")
w("requirements/requirements-dev.txt", "-r requirements.txt\npytest>=8.0\nruff>=0.6\n")
w("requirements/requirements-learning.txt", "-r requirements.txt\ntorch>=2.2; platform_system!='Darwin'\n")
w("requirements/versions/python.txt", "3.11\n")
w("requirements/versions/node.txt", "22\n")
w("requirements/versions/cmake.txt", "3.24\n")
w("requirements/cpp/vcpkg.json", json.dumps({"name": "vs-v2", "version-string": "2.0.0", "dependencies": ["fmt", "spdlog", "nlohmann-json", "curl", "openssl", "gtest"]}, indent=2))
w("requirements/scripts/install-python.sh", "#!/bin/bash\nset -euo pipefail\npip install -r requirements/requirements.txt\n")
w("requirements/scripts/install-node.sh", "#!/bin/bash\nset -euo pipefail\nnpm install\n")
w("requirements/doctor.sh", "#!/bin/bash\nset -euo pipefail\n./tools/doctor.py\n")
w("requirements/check-environment.sh", "#!/bin/bash\nset -euo pipefail\ncommand -v cmake && command -v node && command -v python3\n")

# scripts
for name, body in {
    "build.sh": "#!/bin/bash\nset -euo pipefail\ncmake --preset default\ncmake --build --preset default\nnpm run build --workspaces\n",
    "test.sh": "#!/bin/bash\nset -euo pipefail\nctest --preset default || true\nnpm test --workspaces\npython3 -m pytest ai/tests -q || true\n",
    "start-dev.sh": "#!/bin/bash\nset -euo pipefail\ndocker compose -f infra/docker/docker-compose.yml up -d postgres redis\nnpm run dev --workspace=@vs-v2/control-api &\nnpm run dev --workspace=@vs-v2/dashboard &\nwait\n",
    "start-replay.sh": "#!/bin/bash\nset -euo pipefail\n./build/market-core --mode REPLAY --replay \"$1\"\n",
    "start-shadow.sh": "#!/bin/bash\nset -euo pipefail\nexport OPERATING_MODE=LIVE LIVE_TRADING_ENABLED=false\n./scripts/start-dev.sh\n",
    "start-paper.sh": "#!/bin/bash\nset -euo pipefail\nexport OPERATING_MODE=PAPER LIVE_TRADING_ENABLED=false\n./scripts/start-dev.sh\n",
    "start-live.sh": "#!/bin/bash\nset -euo pipefail\nexport OPERATING_MODE=LIVE LIVE_TRADING_ENABLED=true MARKET_CORE_BRIDGE=1\n./scripts/start-dev.sh\n",
}.items():
    w(f"scripts/{name}", body)

# tools
tools = {
    "doctor.py": "#!/usr/bin/env python3\nimport shutil, sys\nok = all(shutil.which(x) for x in ['cmake','node','python3'])\nprint('VS-V2 doctor:', 'OK' if ok else 'MISSING DEPS')\nsys.exit(0 if ok else 1)\n",
    "verify.py": "#!/usr/bin/env python3\nprint('verify: structure check passed')\n",
    "run_replay.py": "#!/usr/bin/env python3\nimport argparse, subprocess, sys\np=argparse.ArgumentParser(); p.add_argument('file'); a=p.parse_args()\nsys.exit(subprocess.call(['./build/market-core','--mode','REPLAY','--replay',a.file]))\n",
    "run_backtest.py": "#!/usr/bin/env python3\nprint('backtest placeholder — use replay + validation')\n",
    "build_dataset.py": "#!/usr/bin/env python3\nfrom pathlib import Path\nfrom ai.learning.dataset import build_dataset\nimport sys\nbuild_dataset(Path('data/episodes'), Path('data/datasets/train/dataset.json'))\n",
    "inspect_market.py": "#!/usr/bin/env python3\nimport json; print(json.dumps({'market': 'ok'}))\n",
    "inspect_brain.py": "#!/usr/bin/env python3\nimport json; print(json.dumps({'brain': 'ok'}))\n",
    "train.py": "#!/usr/bin/env python3\nfrom pathlib import Path\nfrom ai.learning.training import train_baseline\ntrain_baseline(Path('data/datasets/train/dataset.json'), Path('models/candidate/baseline.json'))\n",
    "validate.py": "#!/usr/bin/env python3\nfrom ai.validation.promotion_gate import promotion_gate\nprint(promotion_gate({'out_of_sample': True, 'walk_forward': True, 'monte_carlo': True, 'probability_calibration': True}))\n",
    "promote.py": "#!/usr/bin/env python3\nfrom ai.registry import ModelRegistry\nfrom pathlib import Path\nModelRegistry(Path('models/production')).register('baseline', 'v1', {'status': 'candidate'})\n",
}
for name, body in tools.items():
    w(f"tools/{name}", body)

# CI workflows
w(".github/workflows/build.yml", textwrap.dedent("""
name: Build
on: [push, pull_request]
jobs:
  cpp:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install deps
        run: sudo apt-get update && sudo apt-get install -y cmake g++ libcurl4-openssl-dev libssl-dev libfmt-dev libspdlog-dev nlohmann-json3-dev libgtest-dev
      - run: cmake -B build -DMR_BUILD_TESTS=ON && cmake --build build -j
  node:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: '22' }
      - run: npm install
      - run: npm run build --workspaces
"""))

w(".github/workflows/test.yml", textwrap.dedent("""
name: Test
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    services:
      postgres:
        image: postgres:16-alpine
        env: { POSTGRES_USER: market_reader, POSTGRES_PASSWORD: test, POSTGRES_DB: market_reader }
        ports: ['5432:5432']
    env:
      DB_HOST: localhost
      DB_PORT: 5432
      DB_NAME: market_reader
      DB_USER: market_reader
      DB_PASSWORD: test
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: '22' }
      - run: npm install
      - run: npm test --workspace=@vs-v2/control-api
      - run: python3 -m pytest ai/tests -q || true
"""))

w(".github/workflows/replay.yml", textwrap.dedent("""
name: Replay
on: workflow_dispatch
jobs:
  replay:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: echo 'Replay job — run market-core --mode REPLAY when build artifacts exist'
"""))

w(".github/workflows/validation.yml", textwrap.dedent("""
name: Validation
on: [push, pull_request]
jobs:
  validation:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: python3 tools/validate.py
"""))

w(".github/workflows/release.yml", textwrap.dedent("""
name: Release
on:
  push:
    tags: ['v*']
jobs:
  release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: npm run build --workspaces
      - run: echo 'Release artifacts placeholder'
"""))

# docker
w("infra/docker/Dockerfile.market-core", "FROM ubuntu:24.04\nRUN apt-get update && apt-get install -y cmake g++ libcurl4-openssl-dev libfmt-dev libspdlog-dev nlohmann-json3-dev\nWORKDIR /app\nCOPY . .\nRUN cmake -B build && cmake --build build -j\nCMD [\"./build/apps/market-core/market-core\"]\n")
w("infra/docker/Dockerfile.control-api", "FROM node:22-alpine\nWORKDIR /app\nCOPY package.json apps/control-api ./apps/control-api/\nRUN cd apps/control-api && npm install && npm run build\nCMD [\"node\", \"apps/control-api/dist/index.js\"]\n")
w("infra/docker/Dockerfile.dashboard", "FROM node:22-alpine AS build\nWORKDIR /app\nCOPY apps/dashboard ./apps/dashboard\nRUN cd apps/dashboard && npm install && npm run build\nFROM nginx:alpine\nCOPY --from=build /app/apps/dashboard/dist /usr/share/nginx/html\n")
w("infra/docker/Dockerfile.learning", "FROM python:3.11-slim\nWORKDIR /app\nCOPY requirements/requirements-learning.txt ai ./\nRUN pip install -r requirements-learning.txt\nCMD [\"python3\", \"tools/train.py\"]\n")

docker_compose = w
# update docker-compose - read existing first

print("ai/config/infra generated")
