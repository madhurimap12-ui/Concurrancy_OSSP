#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -f "$ROOT/runtime/dashboard.pid" ]]; then kill "$(cat "$ROOT/runtime/dashboard.pid")" 2>/dev/null || true; fi
if [[ -f "$ROOT/runtime/server.pid" ]]; then kill -TERM "$(cat "$ROOT/runtime/server.pid")" 2>/dev/null || true; fi
echo "Stop signal sent."
