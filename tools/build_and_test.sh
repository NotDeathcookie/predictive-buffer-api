#!/bin/sh
# Builds and runs every test that does not need the Geode SDK. Needs only a C++20 compiler.
# Usage: tools/build_and_test.sh [CXX]   (default: g++)
set -eu
CXX="${1:-g++}"
cd "$(dirname "$0")/.."
OUT="${TMPDIR:-/tmp}/pbuf-tests"; mkdir -p "$OUT"
FLAGS="-std=c++20 -O1 -Wall -Wextra -Wpedantic -Werror -Iinclude -Isrc/core -Itests"
CORE="src/core/Engine.cpp src/core/Scheduler.cpp"
echo "== public headers are self-contained"
for h in Types Config Presets Strategy Observer TrajectorySearch LegacyCore AdvancedStrategy LevelScan OrbLearning; do
  printf '#include "pbuf/%s.hpp"\nint main(){}\n' "$h" > "$OUT/hdr_$h.cpp"
  $CXX $FLAGS -c "$OUT/hdr_$h.cpp" -o "$OUT/hdr_$h.o"
done
echo "== unit tests";              $CXX $FLAGS tests/test_core.cpp $CORE -o "$OUT/unit" && "$OUT/unit"
echo "== differential test";       $CXX $FLAGS tests/differential_test.cpp $CORE -o "$OUT/diff" && "$OUT/diff"
echo "== zero-allocation hot path"; $CXX $FLAGS tests/alloc_test.cpp $CORE -o "$OUT/alloc" && "$OUT/alloc"
echo "== legacy core (GD-Input-Buffer, ported, used with permission)"
  $CXX $FLAGS tests/legacycore_test.cpp -o "$OUT/legacycore" && "$OUT/legacycore"
echo "== advanced strategy";       $CXX $FLAGS tests/advanced_strategy_test.cpp -o "$OUT/advstrat" && "$OUT/advstrat"
echo "== level scan (synthetic)";  $CXX $FLAGS tests/level_scan_test.cpp -o "$OUT/levelscan" && "$OUT/levelscan"
echo "== orb learning";            $CXX $FLAGS tests/orb_learning_test.cpp -o "$OUT/orblearn" && "$OUT/orblearn"
echo "== C++23 compile check"; $CXX -std=c++23 -Wall -Wextra -Werror -Iinclude -Isrc/core -c src/core/Engine.cpp -o "$OUT/e23.o"
echo "ALL OK"
