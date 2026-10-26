#!/bin/sh
# Type-checks src/geode/{GeodeBackend,Runtime,ApiExports}.cpp and the consumer API against
# tests/geode_stub (a FAKE). Catches typos/signature drift in this repo's own logic only.
# It does NOT prove the code compiles against the real Geode SDK.
set -eu
CXX="${1:-g++}"; cd "$(dirname "$0")/.."
F="-std=c++23 -Wall -Wextra -Werror -fsyntax-only -Itests/geode_stub -Iinclude -Isrc/core -Isrc/geode"
for f in src/geode/GeodeBackend.cpp src/geode/Runtime.cpp src/geode/ApiExports.cpp tests/geode_stub/consumer_smoke.cpp; do
  echo "syntax-check (stub) $f"; $CXX $F "$f"
done
echo "stub syntax check OK (this is NOT a real Geode build)"
