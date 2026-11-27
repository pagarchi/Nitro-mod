#!/bin/sh
# Host-side tests (no GTA, no Plugin-SDK). Usage: sh tests/run_tests.sh   (from the project root)
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
CXX="${CXX:-g++}"; FLAGS="-std=c++14 -Wall -Wextra -g -fsanitize=address,undefined -fno-sanitize-recover=undefined"
echo "== logic tests (Types.cpp + Storage.cpp, ASan/UBSan) =="
$CXX $FLAGS -I"$ROOT/include" -I"$ROOT/include/nitrofx" "$ROOT/tests/logic_test.cpp" "$ROOT/src/Types.cpp" "$ROOT/src/Storage.cpp" -o "$WORK/logic_test"
( cd "$WORK" && ./logic_test )
echo "== MeshPick.cpp against a MOCK RenderWare clump (mock layout is an assumption, not the real SDK) =="
mkdir -p "$WORK/inc/nitrofx" && cp "$ROOT"/include/nitrofx/*.h "$WORK/inc/nitrofx/" && cp "$ROOT/tests/mock_sdk/Pch.h" "$WORK/inc/nitrofx/Pch.h"
$CXX $FLAGS -I"$WORK/inc" -I"$WORK/inc/nitrofx" "$ROOT/tests/meshpick_mock_test.cpp" "$ROOT/src/MeshPick.cpp" "$ROOT/src/Types.cpp" -o "$WORK/meshpick_test"
( cd "$WORK" && ./meshpick_test )
