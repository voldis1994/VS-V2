#!/bin/bash
set -euo pipefail
docker compose -f infra/docker/docker-compose.yml up -d postgres redis
npm run dev --workspace=@vs-v2/control-api &
npm run dev --workspace=@vs-v2/dashboard &
wait
