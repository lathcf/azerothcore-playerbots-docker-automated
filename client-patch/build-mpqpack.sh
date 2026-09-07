#!/usr/bin/env bash
# build-mpqpack.sh - compile client-patch/mpqpack.c AND client-patch/mpqread.c
# against a locally-built StormLib in deps/StormLib, with no sudo/system
# packages required beyond a basic toolchain (git, cmake, gcc/g++, make).
#
# Self-bootstrapping: if deps/StormLib is absent it is cloned and built here
# automatically (STORM_USE_BUNDLED_LIBRARIES=ON avoids a missing system
# libtomcrypt - StormLib compiles its own bundled copy instead of requiring
# the package). Re-runnable; the clone/build steps are skipped once done.
#
# Output: client-patch/mpqpack + client-patch/mpqread (gitignored binaries).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
STORM_DIR="$REPO_ROOT/deps/StormLib"
STORM_BUILD="$STORM_DIR/build"
STORM_LIB="$STORM_BUILD/libstorm.a"

missing=()
for t in git cmake gcc g++ make; do command -v "$t" >/dev/null 2>&1 || missing+=("$t"); done
if (( ${#missing[@]} )); then
    echo "ERROR: missing build tools: ${missing[*]}" >&2
    echo "  Debian/Ubuntu: apt install git cmake gcc g++ make zlib1g-dev libbz2-dev" >&2
    exit 1
fi

if [[ ! -d "$STORM_DIR/src" ]]; then
    echo "==> Cloning StormLib into $STORM_DIR"
    git clone --depth 1 https://github.com/ladislav-zezula/StormLib "$STORM_DIR"
fi

if [[ ! -f "$STORM_LIB" ]]; then
    echo "==> Building StormLib (static, bundled crypto libs)"
    cmake -S "$STORM_DIR" -B "$STORM_BUILD" -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_SHARED_LIBS=OFF -DSTORM_USE_BUNDLED_LIBRARIES=ON
    cmake --build "$STORM_BUILD" -j"$(nproc)"
fi

# -I "$STORM_DIR/src" for StormLib.h; link the static lib + zlib/bzip2
# (StormLib's own libtomcrypt/libtommath are bundled INTO libstorm.a when
# built with -DSTORM_USE_BUNDLED_LIBRARIES=ON, so nothing extra is needed
# for those). -lstdc++ because StormLib itself is C++.
for tool in mpqpack mpqread; do
    echo "==> Compiling $tool"
    gcc -O2 -Wall -Wextra \
        -I "$STORM_DIR/src" \
        "$SCRIPT_DIR/$tool.c" \
        "$STORM_LIB" \
        -lz -lbz2 -lstdc++ \
        -o "$SCRIPT_DIR/$tool"
    echo "Built $SCRIPT_DIR/$tool"
done
