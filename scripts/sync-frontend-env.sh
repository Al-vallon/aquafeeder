#!/usr/bin/env bash
# Copie les variables NEXT_PUBLIC_* de .env.dev / .env.prod vers les fichiers
# locaux lus par Next.js (frontend/.env.development.local / .env.production.local).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:-dev}"
case "$MODE" in
  dev)  SRC="$ROOT/.env.dev";  DST="$ROOT/frontend/.env.development.local" ;;
  prod) SRC="$ROOT/.env.prod"; DST="$ROOT/frontend/.env.production.local" ;;
  *) echo "Usage: $0 {dev|prod}" >&2; exit 1 ;;
esac
grep -E '^NEXT_PUBLIC_' "$SRC" > "$DST"
echo "✅ $DST généré depuis $SRC"
