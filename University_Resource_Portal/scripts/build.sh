#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/runtime" "$ROOT/logs"
gcc -O2 -Wall -Wextra -pthread "$ROOT/server/portal_server.c" -o "$ROOT/server/portal_server"
echo "Build successful: $ROOT/server/portal_server"
