#!/bin/sh
# Builds and runs the unit tests for the platform-independent controller profile code.
# Needs a C++ compiler (g++ or clang++). Run from anywhere:  sh win32/tests/run_tests.sh
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

# Cut the real SJoypad struct out of wsnes9x.h so the test checks the actual field layout.
sed -n '/^struct SJoypad {/,/^};/p' "$here/../wsnes9x.h" > "$out/sjoypad_real.h"
[ -s "$out/sjoypad_real.h" ] || { echo "could not extract struct SJoypad from wsnes9x.h"; exit 1; }

CXX="${CXX:-c++}"
"$CXX" -std=c++11 -Wall -Wextra -DHAVE_REAL_SJOYPAD -I"$out" \
    "$here/inputprofiles_core_test.cpp" "$here/../InputProfilesCore.cpp" -o "$out/test"
"$out/test"
