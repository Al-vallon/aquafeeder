#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
ENV_FILE="${1:-.env.dev}"
export AQUAFEEDER_ENV_FILE="$ENV_FILE"
exec docker compose --env-file "$ENV_FILE" logs -f "$@"
