#!/usr/bin/env bash
# Vérification end-to-end de la stack démarrée.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

ENV_FILE="${1:-.env.dev}"

# Lecture simple sans « source » : les valeurs peuvent contenir des espaces ou parenthèses.
get_var() {
  local key="$1"
  grep -E "^${key}=" "$ENV_FILE" | head -n 1 | cut -d= -f2- | tr -d '\r'
}

API="$(get_var NEXT_PUBLIC_API_BASE_URL)"
FRONTEND_PORT="$(get_var FRONTEND_HOST_PORT)"
ok=0
fail=0

check() {
  local label="$1" url="$2"
  if curl -fsS --max-time 5 "$url" >/dev/null 2>&1; then
    echo "✅ $label"
    ok=$((ok+1))
  else
    echo "❌ $label — $url"
    fail=$((fail+1))
  fi
}

echo "🔍 Smoke test ($ENV_FILE) — API: $API"
check "Frontend"                  "http://localhost:${FRONTEND_PORT}/"
check "API /api/status"           "$API/api/status"
check "API /api/history"          "$API/api/history"
check "API /api/health"           "$API/api/health"
check "API /api/config"           "$API/api/config"
check "API /api/feeds"            "$API/api/feeds"
check "API /api/alerts"           "$API/api/alerts"
check "Actuator /actuator/health" "$API/actuator/health"

if [ "$fail" -gt 0 ]; then
  echo "💥 $fail vérification(s) en échec, $ok réussie(s)."
  exit 1
fi
echo "🎉 Stack opérationnelle ($ok vérifications)."
