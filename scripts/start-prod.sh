#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
export AQUAFEEDER_ENV_FILE=.env.prod
exec docker compose --env-file .env.prod up -d --build --remove-orphans "$@"
