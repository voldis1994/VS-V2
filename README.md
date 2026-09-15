# VS-V2

VS-V2 is a multi-service trading platform: C++ **market-core** runs the live pipeline, **control-api** coordinates brokers/clients/execution, **dashboard** is the operator desk, and **ai/** handles learning and validation.

## Clock domains

`RAW QUOTE` ≠ `FORMING CANDLE` ≠ `CLOSED 10s` ≠ `CLOSED 1m+`.

- Quotes update perception/forming only — **never** structure.
- Closed 10s is a **one-shot** event (`structure_authority=false`).
- **Capital closed 1m+ OHLC** is the sole structure/context authority (`process_authority_ohlc`).
- Decision always scores **LONG, SHORT, WAIT**.
- Risk is **fail-closed** without real account equity (no fake defaults).

## Architecture

```
market-core (C++)
  normalize → quality → fusion → candles → brain → decision → risk intents
       ↓ LIVE bridge (Capital.com)
control-api (Node/Fastify)
  REST + WebSocket: accounts, brokers, market, brain, decisions, positions, risk, …
       ↓
dashboard (React/Vite)
  Overview, Markets, Brain, Decisions, Positions, Risk, Learning, Diagnostics, …
```

| Path | Role |
|------|------|
| `apps/market-core` | C++ pipeline + Capital LIVE bridge → `POST /api/pipeline/intents` |
| `apps/control-api` | `@vs-v2/control-api` — Postgres, Capital.com, websockets |
| `apps/dashboard` | `@vs-v2/dashboard` — dark trading desk UI |
| `ai/` | Feature extraction, training, validation gates |
| `replay/` | Deterministic event replay library |
| `config/` | JSON runtime config (markets, brain, risk, execution, …) |
| `contracts/` | JSON schemas for events and models |
| `database/` | SQL schemas + migrations |
| `infra/docker/` | Dockerfiles + compose (postgres, redis, services) |

## Quick start

```bash
cp .env.example .env
npm install
docker compose -f infra/docker/docker-compose.yml up -d postgres redis
npm run dev --workspace=@vs-v2/control-api
npm run dev --workspace=@vs-v2/dashboard
```

Build C++ (requires cmake, vcpkg deps — see `requirements/cpp/vcpkg.json`):

```bash
cmake -B build -DMR_BUILD_TESTS=ON
cmake --build build -j
./build/apps/market-core/market-core --mode PAPER
```

LIVE bridge (Capital → control-api):

```bash
export PIPELINE_TOKEN=... CAPITAL_API_KEY=... MARKET_CORE_BRIDGE=1
./build/apps/market-core/market-core --mode LIVE --bridge
```

## Scripts

- `scripts/build.sh` — C++ + Node workspaces
- `scripts/start-dev.sh` — postgres/redis + API + dashboard
- `scripts/start-paper.sh` / `start-live.sh` / `start-replay.sh`
- `scripts/deploy-client-web.sh` — build public Client Control Panel (`dist-client`) + gateway on `:5174` (put Cloudflare/nginx HTTPS in front; set `CLIENT_CORS_ORIGIN`, `CLIENT_COOKIE_SECURE=true`, `TRUST_PROXY=true`)
- `tools/environment/doctor.py` — environment check

### Public client web

Admin Control Panel is the dashboard. Remote clients use a separate build:

```bash
npm run build:client --workspace=@vs-v2/dashboard
# or one-shot:
bash scripts/deploy-client-web.sh
```

Gateway serves `apps/dashboard/dist-client` and proxies `/api` + `/ws` to Control API. Clients must use public HTTPS (not Wi‑Fi-only).
